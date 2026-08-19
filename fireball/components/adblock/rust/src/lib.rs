use adblock::lists::{FilterSet, ParseOptions};
use adblock::request::Request;
use adblock::url_parser::{set_domain_resolver, ResolvesDomain};
use adblock::Engine;
use base64::engine::general_purpose::STANDARD as BASE64;
use base64::Engine as _;
use ed25519_dalek::{Signature, Verifier, VerifyingKey};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::ffi::{c_char, CString};
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;
use std::sync::atomic::{AtomicBool, Ordering};

const MAX_RULES_BYTES: usize = 16 * 1024 * 1024;
const MAX_MANIFEST_BYTES: usize = 64 * 1024;
const MAX_URL_BYTES: usize = 8 * 1024;
const MAX_HOST_BYTES: usize = 253;
const MAX_TOKEN_BYTES: usize = 64;
const ENGINE_NAME: &str = "adblock-rust";
const ENGINE_VERSION: &str = "0.13.2";
const SIGNING_CONTEXT: &str = "fireball-adblock-rules-v1";

static DOMAIN_RESOLVER_READY: AtomicBool = AtomicBool::new(false);

pub type FireballDomainResolver = unsafe extern "C" fn(
    hostname_data: *const u8,
    hostname_length: usize,
    domain_start: *mut usize,
    domain_end: *mut usize,
) -> bool;

struct ExternalDomainResolver {
    callback: FireballDomainResolver,
}

impl ResolvesDomain for ExternalDomainResolver {
    fn get_host_domain(&self, host: &str) -> (usize, usize) {
        let mut start = 0;
        let mut end = host.len();
        // SAFETY: Pointers refer to this call's immutable UTF-8 host and local outputs.
        let accepted = unsafe {
            (self.callback)(
                host.as_ptr(),
                host.len(),
                &mut start as *mut usize,
                &mut end as *mut usize,
            )
        };
        if !accepted
            || start > end
            || end > host.len()
            || !host.is_char_boundary(start)
            || !host.is_char_boundary(end)
        {
            return (0, host.len());
        }
        (start, end)
    }
}

pub const FIREBALL_ADBLOCK_STATUS_OK: i32 = 0;
pub const FIREBALL_ADBLOCK_STATUS_INVALID_INPUT: i32 = 1;
pub const FIREBALL_ADBLOCK_STATUS_INTERNAL_ERROR: i32 = 2;

pub const FIREBALL_ADBLOCK_FLAG_BLOCK: u32 = 1 << 0;
pub const FIREBALL_ADBLOCK_FLAG_EXCEPTION: u32 = 1 << 1;
pub const FIREBALL_ADBLOCK_FLAG_IMPORTANT: u32 = 1 << 2;
pub const FIREBALL_ADBLOCK_FLAG_REDIRECT: u32 = 1 << 3;
pub const FIREBALL_ADBLOCK_FLAG_REWRITE: u32 = 1 << 4;

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct RulesManifest {
    schema_version: u32,
    created_at: String,
    minimum_app_version: String,
    engine: ManifestEngine,
    artifact: ManifestArtifact,
    signature: ManifestSignature,
    sources: Vec<ManifestSource>,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ManifestEngine {
    name: String,
    version: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ManifestArtifact {
    url: String,
    size: usize,
    sha256: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ManifestSignature {
    algorithm: String,
    key_id: String,
    value: String,
}

#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct ManifestSource {
    name: String,
    url: String,
    revision: String,
    license: String,
}

#[derive(Serialize)]
struct CosmeticOutput {
    hide_selectors: Vec<String>,
    procedural_actions: Vec<String>,
    exceptions: Vec<String>,
    injected_script: String,
    generichide: bool,
}

pub struct FireballAdblockEngine {
    engine: Engine,
}

#[repr(C)]
pub struct FireballAdblockDecision {
    pub status: i32,
    pub flags: u32,
    pub redirect: *mut c_char,
    pub rewritten_url: *mut c_char,
}

impl FireballAdblockDecision {
    fn invalid() -> Self {
        Self {
            status: FIREBALL_ADBLOCK_STATUS_INVALID_INPUT,
            flags: 0,
            redirect: ptr::null_mut(),
            rewritten_url: ptr::null_mut(),
        }
    }

    fn internal_error() -> Self {
        Self {
            status: FIREBALL_ADBLOCK_STATUS_INTERNAL_ERROR,
            ..Self::invalid()
        }
    }
}

fn is_ascii_token(value: &str, maximum: usize) -> bool {
    !value.is_empty()
        && value.len() <= maximum
        && value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'.' | b'-' | b'_'))
}

fn is_https_url(value: &str) -> bool {
    value.starts_with("https://")
        && value.len() <= MAX_URL_BYTES
        && !value.bytes().any(|byte| byte <= 0x20 || byte == 0x7f)
}

fn is_http_url(value: &str) -> bool {
    (value.starts_with("https://") || value.starts_with("http://"))
        && value.len() <= MAX_URL_BYTES
        && !value.bytes().any(|byte| byte <= 0x20 || byte == 0x7f)
}

fn is_canonical_sha1(value: &str) -> bool {
    value.len() == 40
        && value
            .bytes()
            .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte))
}

fn is_canonical_sha256(value: &str) -> bool {
    value.len() == 64
        && value
            .bytes()
            .all(|byte| byte.is_ascii_digit() || (b'a'..=b'f').contains(&byte))
}

fn version_tuple(value: &str) -> Option<(u32, u32, u32)> {
    let mut segments = value.split('.');
    let major = segments.next()?.parse().ok()?;
    let minor = segments.next()?.parse().ok()?;
    let patch = segments.next()?.parse().ok()?;
    if segments.next().is_some() {
        return None;
    }
    Some((major, minor, patch))
}

fn is_rfc3339_utc(value: &str) -> bool {
    if value.len() != 20
        || !value.ends_with('Z')
        || value.as_bytes().get(4) != Some(&b'-')
        || value.as_bytes().get(7) != Some(&b'-')
        || value.as_bytes().get(10) != Some(&b'T')
        || value.as_bytes().get(13) != Some(&b':')
        || value.as_bytes().get(16) != Some(&b':')
        || !value.bytes().enumerate().all(|(index, byte)| {
            matches!(index, 4 | 7 | 10 | 13 | 16 | 19) || byte.is_ascii_digit()
        })
    {
        return false;
    }
    let parse = |range: std::ops::Range<usize>| value.get(range)?.parse::<u32>().ok();
    matches!(parse(5..7), Some(1..=12))
        && matches!(parse(8..10), Some(1..=31))
        && matches!(parse(11..13), Some(0..=23))
        && matches!(parse(14..16), Some(0..=59))
        && matches!(parse(17..19), Some(0..=59))
}

fn hex_sha256(data: &[u8]) -> String {
    let digest = Sha256::digest(data);
    let mut output = String::with_capacity(64);
    for byte in digest {
        use std::fmt::Write as _;
        let _ = write!(output, "{byte:02x}");
    }
    output
}

fn signing_message(manifest: &RulesManifest) -> String {
    format!(
        "{}\n{}\n{}\n{}\n{}\n{}\n",
        SIGNING_CONTEXT,
        manifest.artifact.size,
        manifest.artifact.sha256,
        manifest.engine.version,
        manifest.created_at,
        manifest.minimum_app_version,
    )
}

fn verify_manifest(
    rules: &[u8],
    manifest_json: &[u8],
    public_key: &[u8],
    current_app_version: &str,
) -> bool {
    if rules.is_empty()
        || rules.len() > MAX_RULES_BYTES
        || manifest_json.is_empty()
        || manifest_json.len() > MAX_MANIFEST_BYTES
        || public_key.len() != 32
    {
        return false;
    }
    let Ok(manifest) = serde_json::from_slice::<RulesManifest>(manifest_json) else {
        return false;
    };
    let Some(minimum_app_version) = version_tuple(&manifest.minimum_app_version) else {
        return false;
    };
    let Some(current_app_version) = version_tuple(current_app_version) else {
        return false;
    };
    if manifest.schema_version != 1
        || !is_rfc3339_utc(&manifest.created_at)
        || current_app_version < minimum_app_version
        || manifest.engine.name != ENGINE_NAME
        || manifest.engine.version != ENGINE_VERSION
        || manifest.artifact.size != rules.len()
        || !is_https_url(&manifest.artifact.url)
        || !is_canonical_sha256(&manifest.artifact.sha256)
        || manifest.artifact.sha256 != hex_sha256(rules)
        || manifest.signature.algorithm != "Ed25519"
        || !is_canonical_sha256(&manifest.signature.key_id)
        || manifest.signature.key_id != hex_sha256(public_key)
        || manifest.sources.is_empty()
        || manifest.sources.len() > 16
    {
        return false;
    }
    if manifest.sources.iter().any(|source| {
        !is_ascii_token(&source.name, 64)
            || !is_https_url(&source.url)
            || !is_canonical_sha1(&source.revision)
            || !is_ascii_token(&source.license, 64)
    }) {
        return false;
    }

    let Ok(signature_bytes) = BASE64.decode(&manifest.signature.value) else {
        return false;
    };
    let Ok(signature) = Signature::from_slice(&signature_bytes) else {
        return false;
    };
    let Ok(key_bytes) = <&[u8; 32]>::try_from(public_key) else {
        return false;
    };
    let Ok(verifying_key) = VerifyingKey::from_bytes(key_bytes) else {
        return false;
    };
    verifying_key
        .verify(signing_message(&manifest).as_bytes(), &signature)
        .is_ok()
}

fn create_engine(rules: &[u8]) -> Option<FireballAdblockEngine> {
    if !DOMAIN_RESOLVER_READY.load(Ordering::Acquire) {
        return None;
    }
    let rules = std::str::from_utf8(rules).ok()?;
    if rules.is_empty() || rules.len() > MAX_RULES_BYTES || rules.contains('\0') {
        return None;
    }
    let mut filter_set = FilterSet::new(false);
    filter_set.add_filter_list(rules.to_owned(), ParseOptions::default());
    Some(FireballAdblockEngine {
        engine: Engine::new_with_filter_set(filter_set),
    })
}

#[no_mangle]
pub extern "C" fn fireball_adblock_set_domain_resolver(
    resolver: Option<FireballDomainResolver>,
) -> bool {
    let Some(callback) = resolver else {
        return false;
    };
    if set_domain_resolver(Box::new(ExternalDomainResolver { callback })).is_err() {
        return false;
    }
    DOMAIN_RESOLVER_READY.store(true, Ordering::Release);
    true
}

unsafe fn read_input<'a>(data: *const u8, length: usize, maximum: usize) -> Option<&'a str> {
    if data.is_null() || length == 0 || length > maximum {
        return None;
    }
    // SAFETY: The FFI contract requires a readable allocation of `length` bytes.
    let bytes = unsafe { std::slice::from_raw_parts(data, length) };
    std::str::from_utf8(bytes).ok()
}

unsafe fn read_bytes<'a>(data: *const u8, length: usize, maximum: usize) -> Option<&'a [u8]> {
    if data.is_null() || length == 0 || length > maximum {
        return None;
    }
    // SAFETY: The FFI contract requires a readable allocation of `length` bytes.
    Some(unsafe { std::slice::from_raw_parts(data, length) })
}

fn into_c_string(value: String) -> *mut c_char {
    CString::new(value)
        .map(CString::into_raw)
        .unwrap_or(ptr::null_mut())
}

fn valid_host(value: &str) -> bool {
    !value.is_empty()
        && value.len() <= MAX_HOST_BYTES
        && !value.starts_with('.')
        && !value.ends_with('.')
        && !value.contains("..")
        && value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || matches!(byte, b'.' | b'-'))
}

fn valid_request_type(value: &str) -> bool {
    matches!(
        value,
        "beacon"
            | "csp"
            | "document"
            | "dtd"
            | "fetch"
            | "font"
            | "image"
            | "media"
            | "object"
            | "other"
            | "ping"
            | "script"
            | "stylesheet"
            | "subdocument"
            | "websocket"
            | "xlst"
            | "xmlhttprequest"
    )
}

fn valid_method(value: &str) -> bool {
    matches!(
        value,
        "GET" | "POST" | "PUT" | "DELETE" | "HEAD" | "OPTIONS" | "PATCH"
    )
}

#[no_mangle]
/// Builds an engine only after the supplied rules manifest is verified.
///
/// # Safety
/// Every non-null input pointer must reference a readable allocation of the
/// corresponding length for the duration of this call.
pub unsafe extern "C" fn fireball_adblock_engine_create_verified(
    rules_data: *const u8,
    rules_length: usize,
    manifest_data: *const u8,
    manifest_length: usize,
    public_key_data: *const u8,
    public_key_length: usize,
    current_app_version_data: *const u8,
    current_app_version_length: usize,
) -> *mut FireballAdblockEngine {
    let result = catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: Inputs are checked for null/size before constructing slices.
        let rules = unsafe { read_bytes(rules_data, rules_length, MAX_RULES_BYTES) }?;
        // SAFETY: Inputs are checked for null/size before constructing slices.
        let manifest = unsafe { read_bytes(manifest_data, manifest_length, MAX_MANIFEST_BYTES) }?;
        // SAFETY: Inputs are checked for null/size before constructing slices.
        let public_key = unsafe { read_bytes(public_key_data, public_key_length, 32) }?;
        // SAFETY: Inputs are checked for null/size before constructing a string.
        let current_app_version = unsafe {
            read_input(
                current_app_version_data,
                current_app_version_length,
                MAX_TOKEN_BYTES,
            )
        }?;
        if !verify_manifest(rules, manifest, public_key, current_app_version) {
            return None;
        }
        create_engine(rules).map(Box::new).map(Box::into_raw)
    }));
    result.ok().flatten().unwrap_or(ptr::null_mut())
}

#[cfg(feature = "ffi-test")]
#[no_mangle]
/// Test-only constructor that bypasses manifest verification.
///
/// # Safety
/// `rules_data` must reference a readable allocation of `rules_length` bytes.
pub unsafe extern "C" fn fireball_adblock_engine_create_unverified_for_testing(
    rules_data: *const u8,
    rules_length: usize,
) -> *mut FireballAdblockEngine {
    let result = catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: Inputs are checked for null/size before constructing slices.
        let rules = unsafe { read_bytes(rules_data, rules_length, MAX_RULES_BYTES) }?;
        create_engine(rules).map(Box::new).map(Box::into_raw)
    }));
    result.ok().flatten().unwrap_or(ptr::null_mut())
}

#[no_mangle]
/// # Safety
/// `engine` must be null or a live pointer returned by this crate. A non-null
/// pointer must be destroyed exactly once and not used afterward.
pub unsafe extern "C" fn fireball_adblock_engine_destroy(engine: *mut FireballAdblockEngine) {
    if !engine.is_null() {
        // SAFETY: The pointer must have been returned by an engine creation function exactly once.
        drop(unsafe { Box::from_raw(engine) });
    }
}

#[allow(clippy::too_many_arguments)]
#[no_mangle]
/// Matches one already-parsed Chromium network request.
///
/// # Safety
/// `engine` must remain live and each input pointer must reference a readable
/// allocation of its declared length. The handle is single-sequence and must
/// not be accessed concurrently.
pub unsafe extern "C" fn fireball_adblock_check_network(
    engine: *const FireballAdblockEngine,
    url_data: *const u8,
    url_length: usize,
    hostname_data: *const u8,
    hostname_length: usize,
    source_hostname_data: *const u8,
    source_hostname_length: usize,
    request_type_data: *const u8,
    request_type_length: usize,
    method_data: *const u8,
    method_length: usize,
    third_party: bool,
) -> FireballAdblockDecision {
    if engine.is_null() {
        return FireballAdblockDecision::invalid();
    }
    let result = catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: Every input is checked for null/size before constructing a string.
        let url = unsafe { read_input(url_data, url_length, MAX_URL_BYTES) }?;
        let hostname = unsafe { read_input(hostname_data, hostname_length, MAX_HOST_BYTES) }?;
        let source_hostname =
            unsafe { read_input(source_hostname_data, source_hostname_length, MAX_HOST_BYTES) }?;
        let request_type =
            unsafe { read_input(request_type_data, request_type_length, MAX_TOKEN_BYTES) }?;
        let method = unsafe { read_input(method_data, method_length, MAX_TOKEN_BYTES) }?;
        if !is_http_url(url)
            || !valid_host(hostname)
            || !valid_host(source_hostname)
            || !valid_request_type(request_type)
            || !valid_method(method)
        {
            return None;
        }
        let request = Request::preparsed(
            url,
            hostname,
            source_hostname,
            request_type,
            third_party,
            method,
        );
        // SAFETY: A non-null engine pointer comes from this crate and remains owned by the caller.
        let matched = unsafe { &*engine }.engine.check_network_request(&request);
        let mut flags = 0;
        if matched.should_block() {
            flags |= FIREBALL_ADBLOCK_FLAG_BLOCK;
        }
        if matched.exception.is_some() {
            flags |= FIREBALL_ADBLOCK_FLAG_EXCEPTION;
        }
        if matched.important {
            flags |= FIREBALL_ADBLOCK_FLAG_IMPORTANT;
        }
        let redirect = matched
            .redirect
            .map(into_c_string)
            .unwrap_or(ptr::null_mut());
        if !redirect.is_null() {
            flags |= FIREBALL_ADBLOCK_FLAG_REDIRECT;
        }
        let rewritten_url = matched
            .rewritten_url
            .map(into_c_string)
            .unwrap_or(ptr::null_mut());
        if !rewritten_url.is_null() {
            flags |= FIREBALL_ADBLOCK_FLAG_REWRITE;
        }
        Some(FireballAdblockDecision {
            status: FIREBALL_ADBLOCK_STATUS_OK,
            flags,
            redirect,
            rewritten_url,
        })
    }));
    match result {
        Ok(Some(decision)) => decision,
        Ok(None) => FireballAdblockDecision::invalid(),
        Err(_) => FireballAdblockDecision::internal_error(),
    }
}

#[no_mangle]
/// # Safety
/// `engine` must be a live, single-sequence handle and `url_data` must be
/// readable for `url_length` bytes.
pub unsafe extern "C" fn fireball_adblock_cosmetic_resources(
    engine: *const FireballAdblockEngine,
    url_data: *const u8,
    url_length: usize,
) -> *mut c_char {
    if engine.is_null() {
        return ptr::null_mut();
    }
    catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: The input is checked for null/size before constructing a string.
        let url = unsafe { read_input(url_data, url_length, MAX_URL_BYTES) }?;
        if !is_http_url(url) {
            return None;
        }
        // SAFETY: A non-null engine pointer comes from this crate and remains owned by the caller.
        let resources = unsafe { &*engine }.engine.url_cosmetic_resources(url);
        let mut hide_selectors: Vec<String> = resources.hide_selectors.into_iter().collect();
        let mut procedural_actions: Vec<String> =
            resources.procedural_actions.into_iter().collect();
        let mut exceptions: Vec<String> = resources.exceptions.into_iter().collect();
        hide_selectors.sort();
        procedural_actions.sort();
        exceptions.sort();
        serde_json::to_string(&CosmeticOutput {
            hide_selectors,
            procedural_actions,
            exceptions,
            injected_script: resources.injected_script,
            generichide: resources.generichide,
        })
        .ok()
        .map(into_c_string)
    }))
    .ok()
    .flatten()
    .unwrap_or(ptr::null_mut())
}

#[no_mangle]
/// # Safety
/// `engine` must be a live, single-sequence handle. Every JSON pointer must
/// reference a readable allocation of the corresponding length.
pub unsafe extern "C" fn fireball_adblock_hidden_selectors(
    engine: *const FireballAdblockEngine,
    classes_json_data: *const u8,
    classes_json_length: usize,
    ids_json_data: *const u8,
    ids_json_length: usize,
    exceptions_json_data: *const u8,
    exceptions_json_length: usize,
) -> *mut c_char {
    if engine.is_null() {
        return ptr::null_mut();
    }
    catch_unwind(AssertUnwindSafe(|| {
        // SAFETY: Inputs are checked for null/size before constructing strings.
        let classes_json =
            unsafe { read_input(classes_json_data, classes_json_length, 256 * 1024) }?;
        let ids_json = unsafe { read_input(ids_json_data, ids_json_length, 256 * 1024) }?;
        let exceptions_json =
            unsafe { read_input(exceptions_json_data, exceptions_json_length, 256 * 1024) }?;
        let classes: Vec<String> = serde_json::from_str(classes_json).ok()?;
        let ids: Vec<String> = serde_json::from_str(ids_json).ok()?;
        let exceptions: std::collections::HashSet<String> =
            serde_json::from_str(exceptions_json).ok()?;
        if classes.len() > 4096
            || ids.len() > 4096
            || exceptions.len() > 4096
            || classes
                .iter()
                .chain(ids.iter())
                .chain(exceptions.iter())
                .any(|value| value.is_empty() || value.len() > 256 || value.contains('\0'))
        {
            return None;
        }
        // SAFETY: A non-null engine pointer comes from this crate and remains owned by the caller.
        let mut selectors =
            unsafe { &*engine }
                .engine
                .hidden_class_id_selectors(&classes, &ids, &exceptions);
        selectors.sort();
        serde_json::to_string(&selectors).ok().map(into_c_string)
    }))
    .ok()
    .flatten()
    .unwrap_or(ptr::null_mut())
}

#[no_mangle]
/// # Safety
/// `value` must be null or a pointer returned by a Fireball adblock string
/// function. A non-null pointer must be released exactly once.
pub unsafe extern "C" fn fireball_adblock_string_destroy(value: *mut c_char) {
    if !value.is_null() {
        // SAFETY: The pointer must have been returned by this crate exactly once.
        drop(unsafe { CString::from_raw(value) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ed25519_dalek::{Signer, SigningKey};
    use serde_json::json;

    const RULES: &str = r#"
||ads.example^
@@||ads.example/allowed.js$script,domain=publisher.example
||tracker.example^$third-party
publisher.example##.sponsored
##.global-ad
"#;

    unsafe extern "C" fn test_domain_resolver(
        hostname_data: *const u8,
        hostname_length: usize,
        domain_start: *mut usize,
        domain_end: *mut usize,
    ) -> bool {
        if hostname_data.is_null() || domain_start.is_null() || domain_end.is_null() {
            return false;
        }
        // SAFETY: The callback contract supplies a readable hostname buffer.
        let hostname = unsafe { std::slice::from_raw_parts(hostname_data, hostname_length) };
        let Some(last_dot) = hostname.iter().rposition(|byte| *byte == b'.') else {
            // SAFETY: Output pointers are non-null and valid for this callback.
            unsafe {
                *domain_start = 0;
                *domain_end = hostname_length;
            }
            return true;
        };
        let prefix = &hostname[..last_dot];
        let start = prefix
            .iter()
            .rposition(|byte| *byte == b'.')
            .map_or(0, |position| position + 1);
        // SAFETY: Output pointers are non-null and valid for this callback.
        unsafe {
            *domain_start = start;
            *domain_end = hostname_length;
        }
        true
    }

    fn ensure_domain_resolver() {
        static ONCE: std::sync::Once = std::sync::Once::new();
        ONCE.call_once(|| {
            assert!(fireball_adblock_set_domain_resolver(Some(
                test_domain_resolver
            )))
        });
    }

    fn decision(engine: &FireballAdblockEngine, url: &str, third_party: bool) -> bool {
        let request = Request::preparsed(
            url,
            if url.contains("tracker") {
                "tracker.example"
            } else {
                "ads.example"
            },
            "publisher.example",
            "script",
            third_party,
            "GET",
        );
        engine.engine.check_network_request(&request).should_block()
    }

    #[test]
    fn network_rules_and_exceptions_are_real_engine_results() {
        ensure_domain_resolver();
        let engine = create_engine(RULES.as_bytes()).expect("engine");
        assert!(decision(&engine, "https://ads.example/banner.js", true));
        assert!(!decision(&engine, "https://ads.example/allowed.js", true));
        assert!(decision(&engine, "https://tracker.example/pixel.js", true));
        assert!(!decision(
            &engine,
            "https://tracker.example/pixel.js",
            false
        ));
    }

    #[test]
    fn cosmetic_and_generic_selectors_are_available() {
        ensure_domain_resolver();
        let engine = create_engine(RULES.as_bytes()).expect("engine");
        let resources = engine
            .engine
            .url_cosmetic_resources("https://publisher.example/article");
        assert!(resources.hide_selectors.contains(".sponsored"));
        let hidden = engine.engine.hidden_class_id_selectors(
            ["global-ad"],
            std::iter::empty::<&str>(),
            &resources.exceptions,
        );
        assert!(hidden.contains(&".global-ad".to_string()));
    }

    #[test]
    fn verified_manifest_rejects_tampering() {
        ensure_domain_resolver();
        let signing_key = SigningKey::from_bytes(&[7_u8; 32]);
        let public_key = signing_key.verifying_key().to_bytes();
        let rules_sha = hex_sha256(RULES.as_bytes());
        let mut manifest = json!({
            "schema_version": 1,
            "created_at": "2026-08-20T00:00:00Z",
            "minimum_app_version": "0.1.0",
            "engine": {"name": ENGINE_NAME, "version": ENGINE_VERSION},
            "artifact": {
                "url": "https://updates.fireball.example/adblock/rules.txt",
                "size": RULES.len(),
                "sha256": rules_sha,
            },
            "signature": {
                "algorithm": "Ed25519",
                "key_id": hex_sha256(&public_key),
                "value": "pending",
            },
            "sources": [{
                "name": "easylist",
                "url": "https://github.com/easylist/easylist.git",
                "revision": "0123456789abcdef0123456789abcdef01234567",
                "license": "GPL-3.0-or-later",
            }],
        });
        let parsed: RulesManifest = serde_json::from_value(manifest.clone()).expect("manifest");
        let signature = signing_key.sign(signing_message(&parsed).as_bytes());
        manifest["signature"]["value"] = json!(BASE64.encode(signature.to_bytes()));
        let encoded = serde_json::to_vec(&manifest).expect("json");
        assert!(verify_manifest(
            RULES.as_bytes(),
            &encoded,
            &public_key,
            "0.1.0"
        ));
        let engine = unsafe {
            fireball_adblock_engine_create_verified(
                RULES.as_ptr(),
                RULES.len(),
                encoded.as_ptr(),
                encoded.len(),
                public_key.as_ptr(),
                public_key.len(),
                b"0.1.0".as_ptr(),
                b"0.1.0".len(),
            )
        };
        assert!(!engine.is_null());
        unsafe { fireball_adblock_engine_destroy(engine) };
        assert!(!verify_manifest(
            RULES.as_bytes(),
            &encoded,
            &public_key,
            "0.0.9"
        ));

        let mut tampered = RULES.as_bytes().to_vec();
        tampered.push(b'!');
        assert!(!verify_manifest(&tampered, &encoded, &public_key, "0.1.0"));
        manifest["minimum_app_version"] = json!("invalid");
        assert!(!verify_manifest(
            RULES.as_bytes(),
            &serde_json::to_vec(&manifest).expect("json"),
            &public_key,
            "0.1.0",
        ));
    }
}
