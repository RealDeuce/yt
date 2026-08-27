#!/usr/bin/env python3
"""Regenerate the finite Yankee Trader native implementation ledger.

The reverse-engineering repository owns the legacy denominators.  This tool
copies their stable identities into this implementation tree and deliberately
starts unmapped native obligations as ``missing``.  A test merely existing for
an executable is not enough to mark one legacy root or transfer verified.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
from typing import Any


DEFAULT_ANALYSIS = Path("/bbsdev/doors/yt/3.6G")


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as source:
        return json.load(source)


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def write_tsv(path: Path, fields: list[str], rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=fields, dialect="excel-tab", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


PROGRAM_CANDIDATES = {
    "local.exe": ("src/main_local.c::main", "tests/test_utilities.c::test_local"),
    "portname.exe": (
        "src/main_portname.c::main",
        "tests/test_utilities.c::test_portname",
    ),
    "rmt-init.exe": (
        "src/main_rmt_init.c::main; src/yt_init.c",
        "tests/test_utilities.c::test_rmt_init",
    ),
    "yt-init.exe": (
        "src/main_yt_init.c::main; src/yt_init.c",
        "tests/test_clean_install.c::main",
    ),
    "yt.exe": (
        "src/main_yt.c::main; src/yt_door.c; src/yt_session.c",
        "(compositional component fixtures; no monolithic session test)",
    ),
    "ytconfig.exe": (
        "src/main_ytconfig.c::main",
        "tests/test_utilities.c::test_ytconfig",
    ),
    "ytmaint.exe": (
        "src/yt_maint.c::yt_maintenance_run",
        "tests/test_clean_install.c::main (fresh-install path only)",
    ),
}


def root_rows(root_data: dict[str, Any]) -> list[dict[str, Any]]:
    rows = []
    for root in root_data["roots"]:
        if root["disposition"] == "excluded_uncalled_module_seed":
            continue
        is_entry = root["kind"] == "entry"
        candidate, test = PROGRAM_CANDIDATES[root["executable"]]
        rows.append(
            {
                "legacy_root": root["id"],
                "executable": root["executable"],
                "kind": root["kind"],
                "legacy_nodes": root["nodes"],
                "analysis_evidence": "; ".join(root["evidence"]),
                "native_function": candidate if is_entry else "(missing mapping)",
                "native_test_or_fixture": test if is_entry else "(missing)",
                "status": "candidate" if is_entry else "missing",
                "prerequisites": (
                    "map child roots and all outgoing transfers; exact external adapters"
                    if is_entry
                    else "identify exact C owner; map outgoing transfers; add exact test"
                ),
            }
        )
    return rows


def transfer_rows(cfg: dict[str, Any]) -> list[dict[str, Any]]:
    rows = []
    for ordinal, edge in enumerate(cfg["transfers"], start=1):
        targets = ",".join(edge["targets"])
        stable_id = f"edge-{ordinal:04d}"
        rows.append(
            {
                "edge_id": stable_id,
                "executable": edge["program"],
                "legacy_source": edge["source"],
                "kind": edge["kind"],
                "legacy_targets": targets,
                "legacy_continuation": edge["continuation"] or "",
                "operation": edge["operation"],
                "native_function": "(missing mapping)",
                "native_test_or_fixture": "(missing)",
                "status": "missing",
                "prerequisites": "map owning native branch and exact state/output/effect test",
            }
        )
    return rows


def presentation_rows(output_data: dict[str, Any]) -> list[dict[str, Any]]:
    site_counts: dict[str, int] = {}
    for site in output_data["presentation_sites"]:
        family = site["display_family"]
        site_counts[family] = site_counts.get(family, 0) + 1

    rows = []
    for family in output_data["display_families"]:
        aggregate = family["status"] == "aggregate_catalog"
        rows.append(
            {
                "component": family["id"],
                "audience": family["audience"],
                "legacy_site_count": site_counts.get(family["id"], 0),
                "analysis_status": family["status"],
                "analysis_scope": family.get(
                    "scope", "aggregate catalog; owns no presentation sites"
                ),
                "analysis_evidence": "; ".join(family["evidence"]),
                "native_function": "(catalog only)" if aggregate else "(missing mapping)",
                "native_test_or_fixture": "(not applicable)" if aggregate else "(missing)",
                "status": "verified" if aggregate else "missing",
                "prerequisites": (
                    "none; aggregate owns no presentation sites"
                    if aggregate
                    else "identify exact C owner; exact remote bytes and local result; edge tests"
                ),
            }
        )
    return rows


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--analysis-root", type=Path, default=DEFAULT_ANALYSIS)
    parser.add_argument(
        "--output", type=Path, default=Path("implementation-coverage")
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    analysis = arguments.analysis_root
    artifacts = analysis / "analysis" / "disassembly"
    source_paths = {
        "completion": artifacts / "completion-ledger.json",
        "roots": artifacts / "root-coverage.json",
        "transfers": artifacts / "qualified-cfg.json",
        "presentation": artifacts / "player-output-coverage.json",
    }
    loaded = {name: load_json(path) for name, path in source_paths.items()}

    roots = root_rows(loaded["roots"])
    transfers = transfer_rows(loaded["transfers"])
    presentation = presentation_rows(loaded["presentation"])

    expected = {
        "roots": 168,
        "transfers": 7368,
        "presentation_families": 150,
        "presentation_sites": 3815,
        "literal_references": 2060,
    }
    actual = {
        "roots": len(roots),
        "transfers": len(transfers),
        "presentation_families": len(presentation),
        "presentation_sites": loaded["presentation"]["totals"]["presentation_sites"],
        "literal_references": loaded["presentation"]["totals"][
            "literal_candidate_xrefs"
        ],
    }
    if actual != expected:
        raise SystemExit(f"final-analysis denominator changed: {actual!r} != {expected!r}")

    arguments.output.mkdir(parents=True, exist_ok=True)
    write_tsv(
        arguments.output / "roots.tsv",
        [
            "legacy_root",
            "executable",
            "kind",
            "legacy_nodes",
            "analysis_evidence",
            "native_function",
            "native_test_or_fixture",
            "status",
            "prerequisites",
        ],
        roots,
    )
    write_tsv(
        arguments.output / "transfers.tsv",
        [
            "edge_id",
            "executable",
            "legacy_source",
            "kind",
            "legacy_targets",
            "legacy_continuation",
            "operation",
            "native_function",
            "native_test_or_fixture",
            "status",
            "prerequisites",
        ],
        transfers,
    )
    write_tsv(
        arguments.output / "presentation.tsv",
        [
            "component",
            "audience",
            "legacy_site_count",
            "analysis_status",
            "analysis_scope",
            "analysis_evidence",
            "native_function",
            "native_test_or_fixture",
            "status",
            "prerequisites",
        ],
        presentation,
    )

    manifest = {
        "schema_version": 1,
        "source_authority": str(analysis),
        "source_artifacts": {
            name: {"path": str(path), "sha256": digest(path)}
            for name, path in source_paths.items()
        },
        "denominators": actual,
        "policy": {
            "statuses": ["missing", "candidate", "verified", "explicitly deferred"],
            "no_combined_percentage": True,
            "edge_credit_requires_exact_native_mapping_and_test": True,
        },
    }
    with (arguments.output / "manifest.json").open("w", encoding="utf-8") as output:
        json.dump(manifest, output, indent=2, sort_keys=True)
        output.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
