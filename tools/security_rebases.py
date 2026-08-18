#!/usr/bin/env python3
"""Validate evidence for Fireball Chromium security rebases."""

from __future__ import annotations

import argparse
import datetime
import json
import pathlib
import re
import sys
from typing import Any

EVENT_ID_PATTERN = re.compile(r"^m\d{2,3}-\d{4}-\d{2}-\d{2}(?:-[a-z0-9-]+)?$")
SHA1_PATTERN = re.compile(r"^[0-9a-f]{40}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
VERSION_PATTERN = re.compile(r"^(\d+)\.\d+\.\d+\.\d+$")
RESULT_FIELDS = {
    "control_build",
    "overlay_build",
    "smoke_tests",
    "startup_network_audit",
}


class RebaseError(ValueError):
    pass


def parse_time(value: object, field: str) -> datetime.datetime:
    if not isinstance(value, str):
        raise RebaseError(f"{field}: timestamp is required")
    try:
        parsed = datetime.datetime.fromisoformat(value)
    except ValueError as error:
        raise RebaseError(f"{field}: timestamp must use ISO 8601") from error
    if parsed.tzinfo is None:
        raise RebaseError(f"{field}: timestamp must include a timezone")
    return parsed


def load_ledger(path: pathlib.Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        document = json.load(handle)
    if not isinstance(document, dict):
        raise RebaseError("ledger root must be an object")
    return document


def validate_ledger(document: dict[str, Any]) -> None:
    if set(document) != {"schema_version", "sla_hours", "required_consecutive_passes", "events"}:
        raise RebaseError("ledger has an unexpected shape")
    if document["schema_version"] != 1:
        raise RebaseError("unsupported ledger schema")
    if document["sla_hours"] != 72:
        raise RebaseError("security rebase SLA must remain 72 hours")
    if document["required_consecutive_passes"] != 2:
        raise RebaseError("Linux alpha requires two consecutive passing rebases")
    events = document["events"]
    if not isinstance(events, list):
        raise RebaseError("events must be an array")

    expected_fields = {
        "id",
        "version",
        "milestone",
        "chromium_revision",
        "upstream_released_at",
        "triaged_at",
        "rebase_completed_at",
        "artifact_promoted_at",
        "artifact_sha256",
        "outcome",
        "failure_reason",
        "results",
    }
    seen_ids: set[str] = set()
    seen_revisions: set[str] = set()
    previous_release: datetime.datetime | None = None
    for index, event in enumerate(events):
        if not isinstance(event, dict) or set(event) != expected_fields:
            raise RebaseError(f"event[{index}] has an unexpected shape")
        event_id = event["id"]
        if not isinstance(event_id, str) or EVENT_ID_PATTERN.fullmatch(event_id) is None:
            raise RebaseError(f"event[{index}].id is invalid")
        if event_id in seen_ids:
            raise RebaseError(f"duplicate event id: {event_id}")
        seen_ids.add(event_id)

        version_match = VERSION_PATTERN.fullmatch(str(event["version"]))
        if version_match is None or event["milestone"] != int(version_match.group(1)):
            raise RebaseError(f"{event_id}: milestone does not match version")
        revision = event["chromium_revision"]
        if not isinstance(revision, str) or SHA1_PATTERN.fullmatch(revision) is None:
            raise RebaseError(f"{event_id}: Chromium revision must be an exact commit")
        if revision in seen_revisions:
            raise RebaseError(f"duplicate Chromium revision: {revision}")
        seen_revisions.add(revision)

        released = parse_time(event["upstream_released_at"], f"{event_id}.upstream_released_at")
        triaged = parse_time(event["triaged_at"], f"{event_id}.triaged_at")
        completed_value = event["rebase_completed_at"]
        promoted_value = event["artifact_promoted_at"]
        completed = (
            parse_time(completed_value, f"{event_id}.rebase_completed_at")
            if completed_value is not None
            else None
        )
        promoted = (
            parse_time(promoted_value, f"{event_id}.artifact_promoted_at")
            if promoted_value is not None
            else None
        )
        timeline = [timestamp for timestamp in (released, triaged, completed, promoted) if timestamp]
        if any(left > right for left, right in zip(timeline, timeline[1:])):
            raise RebaseError(f"{event_id}: timestamps are out of order")
        if previous_release is not None and released <= previous_release:
            raise RebaseError(f"{event_id}: events must be ordered by upstream release time")
        previous_release = released

        results = event["results"]
        if not isinstance(results, dict) or set(results) != RESULT_FIELDS:
            raise RebaseError(f"{event_id}: results have an unexpected shape")
        if any(result not in {"passed", "failed", "not_run"} for result in results.values()):
            raise RebaseError(f"{event_id}: invalid check result")

        outcome = event["outcome"]
        failure_reason = event["failure_reason"]
        artifact_sha256 = event["artifact_sha256"]
        if outcome == "passed":
            if completed is None or promoted is None:
                raise RebaseError(f"{event_id}: a passing event requires completion and promotion times")
            if promoted - released > datetime.timedelta(hours=document["sla_hours"]):
                raise RebaseError(f"{event_id}: security rebase exceeded the 72-hour SLA")
            if not isinstance(artifact_sha256, str) or SHA256_PATTERN.fullmatch(artifact_sha256) is None:
                raise RebaseError(f"{event_id}: artifact SHA-256 is invalid")
            failed = sorted(name for name, result in results.items() if result != "passed")
            if failed:
                raise RebaseError(f"{event_id}: passing event has incomplete checks: {', '.join(failed)}")
            if failure_reason is not None:
                raise RebaseError(f"{event_id}: passing event cannot have a failure reason")
        elif outcome == "failed":
            if not isinstance(failure_reason, str) or not failure_reason.strip():
                raise RebaseError(f"{event_id}: failed event requires a reason")
            if (promoted is None) != (artifact_sha256 is None):
                raise RebaseError(f"{event_id}: promotion time and artifact checksum must appear together")
            if artifact_sha256 is not None and (
                not isinstance(artifact_sha256, str) or SHA256_PATTERN.fullmatch(artifact_sha256) is None
            ):
                raise RebaseError(f"{event_id}: artifact SHA-256 is invalid")
            missed_sla = promoted is not None and (
                promoted - released > datetime.timedelta(hours=document["sla_hours"])
            )
            if all(result == "passed" for result in results.values()) and not missed_sla:
                raise RebaseError(f"{event_id}: failed event must record a failed check or SLA miss")
        else:
            raise RebaseError(f"{event_id}: outcome must be passed or failed")


def gate_status(document: dict[str, Any]) -> dict[str, int | bool]:
    validate_ledger(document)
    completed = 0
    for event in reversed(document["events"]):
        if event["outcome"] != "passed":
            break
        completed += 1
    required = document["required_consecutive_passes"]
    return {
        "completed_consecutive_passes": min(completed, required),
        "required_consecutive_passes": required,
        "linux_alpha_gate_ready": completed >= required,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("check", "status"))
    parser.add_argument(
        "--ledger",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1] / "security/rebases.json",
    )
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        status = gate_status(load_ledger(arguments.ledger))
        if arguments.command == "status":
            print(json.dumps(status, sort_keys=True))
    except (OSError, json.JSONDecodeError, RebaseError) as error:
        print(f"fireball-security-rebases: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
