from __future__ import annotations

from .pipeline import build_parser, run


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    output = run(args)
    print(output)
