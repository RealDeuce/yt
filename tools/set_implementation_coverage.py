#!/usr/bin/env python3
"""Update native annotations for exact implementation-ledger identities."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


LEDGERS = {
    "roots": (Path("implementation-coverage/roots.tsv"), "legacy_root"),
    "transfers": (Path("implementation-coverage/transfers.tsv"), "edge_id"),
    "presentation": (Path("implementation-coverage/presentation.tsv"), "component"),
}


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("ledger", choices=LEDGERS)
    parser.add_argument("identity", nargs="+")
    parser.add_argument("--native-function")
    parser.add_argument("--native-test")
    parser.add_argument(
        "--status",
        choices=("missing", "candidate", "verified", "explicitly deferred"),
    )
    parser.add_argument("--prerequisites")
    arguments = parser.parse_args()
    if all(
        value is None
        for value in (
            arguments.native_function,
            arguments.native_test,
            arguments.status,
            arguments.prerequisites,
        )
    ):
        parser.error("at least one native annotation must be supplied")
    return arguments


def main() -> int:
    arguments = parse_arguments()
    path, key = LEDGERS[arguments.ledger]
    with path.open("r", encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, dialect="excel-tab")
        if reader.fieldnames is None:
            raise SystemExit(f"invalid ledger: {path}")
        fields = reader.fieldnames
        rows = list(reader)

    requested = set(arguments.identity)
    found: set[str] = set()
    for row in rows:
        identity = row[key]
        if identity not in requested:
            continue
        if identity in found:
            raise SystemExit(f"duplicate ledger identity: {identity}")
        found.add(identity)
        if arguments.native_function is not None:
            row["native_function"] = arguments.native_function
        if arguments.native_test is not None:
            row["native_test_or_fixture"] = arguments.native_test
        if arguments.status is not None:
            row["status"] = arguments.status
        if arguments.prerequisites is not None:
            row["prerequisites"] = arguments.prerequisites

    missing = requested - found
    if missing:
        raise SystemExit(f"unknown ledger identities: {', '.join(sorted(missing))}")

    temporary = path.with_suffix(path.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=fields, dialect="excel-tab", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)
    temporary.replace(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
