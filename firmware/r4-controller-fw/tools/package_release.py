#!/usr/bin/env python3

import argparse
import hashlib
import pathlib
import shutil


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--uf2", required=True, type=pathlib.Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    args = parser.parse_args()

    source = args.uf2.resolve()
    if not source.is_file():
        raise SystemExit(f"UF2 does not exist: {source}")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    filename = f"r4-controller-fw-{args.version}.uf2"
    destination = args.output_dir / filename
    shutil.copyfile(source, destination)

    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    manifest = destination.with_suffix(destination.suffix + ".manifest")
    checksum = destination.with_suffix(destination.suffix + ".sha256")

    manifest.write_text(
        "\n".join(
            (
                "R4_UF2_PRODUCT=R4 Controller",
                "R4_UF2_FAMILY_ID=0xE48BFF56",
                f"R4_FIRMWARE_VERSION={args.version}",
                f"R4_UF2_FILE={filename}",
                f"R4_UF2_SHA256={digest}",
                "",
            )
        ),
        encoding="ascii",
        newline="\n",
    )
    checksum.write_text(
        f"{digest}  {filename}\n",
        encoding="ascii",
        newline="\n",
    )
    print(destination)
    print(manifest)
    print(checksum)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
