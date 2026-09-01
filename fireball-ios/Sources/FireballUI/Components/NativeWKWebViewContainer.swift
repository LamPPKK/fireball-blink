import SwiftUI
import WebKit
import FireballCore

#if canImport(UIKit)
import UIKit

/// SwiftUI wrapper around Apple's native WebKit `WKWebView` with Fireball Shields and Space isolation.
public struct NativeWKWebViewContainer: UIViewRepresentable {
    public let url: URL
    public let spaceId: String
    public let isBurner: Bool
    public let shieldsEnabled: Bool
    @Binding public var estimatedProgress: Double
    @Binding public var currentTitle: String
    @Binding public var canGoBack: Bool
    @Binding public var canGoForward: Bool

    public init(
        url: URL,
        spaceId: String,
        isBurner: Bool = false,
        shieldsEnabled: Bool = true,
        estimatedProgress: Binding<Double>,
        currentTitle: Binding<String>,
        canGoBack: Binding<Bool>,
        canGoForward: Binding<Bool>
    ) {
        self.url = url
        self.spaceId = spaceId
        self.isBurner = isBurner
        self.shieldsEnabled = shieldsEnabled
        self._estimatedProgress = estimatedProgress
        self._currentTitle = currentTitle
        self._canGoBack = canGoBack
        self._canGoForward = canGoForward
    }

    public func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    public func makeUIView(context: Context) -> WKWebView {
        let configuration = WKWebViewConfiguration()

        // 1. Session Isolation per Space
        if isBurner {
            configuration.websiteDataStore = WKWebsiteDataStore.nonPersistent()
        } else {
            configuration.websiteDataStore = WKWebsiteDataStore.default()
        }

        // 2. Fireball Shields: Cosmetic Adblock Script Injection
        if shieldsEnabled {
            let cosmeticCSS = """
            const style = document.createElement('style');
            style.textContent = '.ad-banner, .adsbox, [class*="ad-container"] { display: none !important; }';
            document.head.appendChild(style);
            """
            let userScript = WKUserScript(
                source: cosmeticCSS,
                injectionTime: .atDocumentEnd,
                forMainFrameOnly: false
            )
            configuration.userContentController.addUserScript(userScript)
        }

        let webView = WKWebView(frame: .zero, configuration: configuration)
        webView.navigationDelegate = context.coordinator
        webView.allowsBackForwardNavigationGestures = true

        // Observe progress and title
        context.coordinator.setupObservers(for: webView)

        let cleanUrl = shieldsEnabled ? CleanURLHelper.clean(url.absoluteString) : url.absoluteString
        if let targetURL = URL(string: cleanUrl) {
            webView.load(URLRequest(url: targetURL))
        }

        return webView
    }

    public func updateUIView(_ uiView: WKWebView, context: Context) {
        // Handle external URL changes or navigation controls
    }

    public class Coordinator: NSObject, WKNavigationDelegate {
        var parent: NativeWKWebViewContainer
        private var progressObservation: NSKeyValueObservation?
        private var titleObservation: NSKeyValueObservation?
        private var canGoBackObservation: NSKeyValueObservation?
        private var canGoForwardObservation: NSKeyValueObservation?

        init(_ parent: NativeWKWebViewContainer) {
            self.parent = parent
        }

        func setupObservers(for webView: WKWebView) {
            progressObservation = webView.observe(\.estimatedProgress, options: [.new]) { [weak self] webView, _ in
                DispatchQueue.main.async {
                    self?.parent.estimatedProgress = webView.estimatedProgress
                }
            }

            titleObservation = webView.observe(\.title, options: [.new]) { [weak self] webView, _ in
                DispatchQueue.main.async {
                    self?.parent.currentTitle = webView.title ?? ""
                }
            }

            canGoBackObservation = webView.observe(\.canGoBack, options: [.new]) { [weak self] webView, _ in
                DispatchQueue.main.async {
                    self?.parent.canGoBack = webView.canGoBack
                }
            }

            canGoForwardObservation = webView.observe(\.canGoForward, options: [.new]) { [weak self] webView, _ in
                DispatchQueue.main.async {
                    self?.parent.canGoForward = webView.canGoForward
                }
            }
        }

        public func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            if let requestURL = navigationAction.request.url?.absoluteString, parent.shieldsEnabled {
                let cleaned = CleanURLHelper.clean(requestURL)
                if cleaned != requestURL, let targetURL = URL(string: cleaned) {
                    decisionHandler(.cancel)
                    webView.load(URLRequest(url: targetURL))
                    return
                }
            }
            decisionHandler(.allow)
        }
    }
}
#endif
