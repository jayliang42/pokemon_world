#!/usr/bin/env python3
"""Generate the original low-poly Eevee-inspired field model.

The asset is built only from editable primitive geometry in this repository.
It is a new stylized model, not geometry or textures extracted from a
commercial Pokemon game.
"""

from __future__ import annotations

from pathlib import Path

from generate_bulbasaur import MeshPart, add_ellipsoid, add_pyramid, dot


def build_parts() -> list[MeshPart]:
    parts: dict[str, MeshPart] = {}

    def part(name: str) -> MeshPart:
        return parts.setdefault(name, MeshPart(name))

    body = part("body")
    add_ellipsoid(body, (0.0, 0.42, -0.12), (0.36, 0.29, 0.48), rings=5)
    add_ellipsoid(body, (0.0, 0.68, 0.30), (0.31, 0.34, 0.31), rings=5)
    add_ellipsoid(body, (0.0, 0.71, 0.54), (0.27, 0.22, 0.20), rings=4)

    mane = part("mane")
    add_ellipsoid(mane, (0.0, 0.56, 0.18), (0.42, 0.16, 0.40), rings=4)
    for side in (-1.0, 1.0):
        add_pyramid(
            mane,
            (side * 0.23, 0.52, 0.32),
            (side * 0.36, 0.37, 0.47),
            0.09,
            0.11,
        )

    for side, name in ((-1.0, "ear-left"), (1.0, "ear-right")):
        ear = part(name)
        add_ellipsoid(
            ear,
            (side * 0.20, 1.12, 0.31),
            (0.12, 0.37, 0.115),
            rotation=(0.0, side * 0.12, side * -0.12),
            rings=4,
            segments=8,
        )
        add_pyramid(
            ear,
            (side * 0.20, 1.39, 0.31),
            (side * 0.24, 1.65, 0.34),
            0.075,
            0.07,
        )
        inner = part("ear-inner")
        add_ellipsoid(
            inner,
            (side * 0.20, 1.13, 0.41),
            (0.052, 0.22, 0.018),
            rotation=(0.0, side * 0.12, side * -0.12),
            rings=3,
            segments=7,
        )

    leg_positions = {
        "leg-front-left": (-0.22, 0.22, 0.27),
        "leg-front-right": (0.22, 0.22, 0.27),
        "leg-back-left": (-0.22, 0.21, -0.30),
        "leg-back-right": (0.22, 0.21, -0.30),
    }
    for name, center in leg_positions.items():
        leg = part(name)
        add_ellipsoid(leg, center, (0.12, 0.22, 0.13), rings=4, segments=8)
        add_ellipsoid(
            leg,
            (center[0], 0.075, center[2] + 0.042),
            (0.15, 0.075, 0.17),
            rings=4,
            segments=8,
        )

    tail = part("tail")
    add_ellipsoid(
        tail,
        (0.0, 0.53, -0.62),
        (0.18, 0.16, 0.42),
        rotation=(0.16, 0.0, 0.0),
        rings=4,
        segments=8,
    )
    add_pyramid(tail, (0.0, 0.47, -0.94), (0.0, 0.63, -1.22), 0.13, 0.13)

    tail_tip = part("tail-tip")
    add_ellipsoid(
        tail_tip,
        (0.0, 0.58, -1.03),
        (0.13, 0.13, 0.20),
        rotation=(0.16, 0.0, 0.0),
        rings=4,
        segments=8,
    )

    eye_white = part("eye-white")
    eye_iris = part("eye-iris")
    eye_dark = part("eye-dark")
    eye_highlight = part("eye-highlight")
    for side in (-1.0, 1.0):
        add_ellipsoid(
            eye_white, (side * 0.145, 0.77, 0.735), (0.09, 0.105, 0.018),
            rings=4, segments=8
        )
        add_ellipsoid(
            eye_iris, (side * 0.145, 0.765, 0.756), (0.052, 0.070, 0.013),
            rings=4, segments=8
        )
        add_ellipsoid(
            eye_dark, (side * 0.145, 0.758, 0.770), (0.024, 0.041, 0.009),
            rings=3, segments=7
        )
        add_ellipsoid(
            eye_highlight, (side * 0.130, 0.785, 0.780), (0.011, 0.015, 0.005),
            rings=3, segments=6
        )
    add_ellipsoid(eye_dark, (0.0, 0.62, 0.769), (0.050, 0.028, 0.007), rings=3, segments=7)

    return list(parts.values())


def validate_parts(parts: list[MeshPart]) -> None:
    expected = {
        "body", "mane", "ear-left", "ear-right", "ear-inner",
        "leg-front-left", "leg-front-right", "leg-back-left", "leg-back-right",
        "tail", "tail-tip", "eye-white", "eye-iris", "eye-dark", "eye-highlight",
    }
    names = {part.name for part in parts}
    if names != expected:
        raise ValueError(f"unexpected Eevee animation groups: {sorted(names)}")
    for part in parts:
        if not part.vertices or not part.faces or len(part.vertices) != len(part.normals):
            raise ValueError(f"invalid Eevee mesh group: {part.name}")
        for normal in part.normals:
            if abs(dot(normal, normal) - 1.0) > 1.0e-4:
                raise ValueError(f"non-unit normal in {part.name}")


def write_obj(path: Path, parts: list[MeshPart]) -> None:
    lines = [
        "# Pokemon World original procedural low-poly Eevee-inspired creature",
        "# Regenerate with: python3 tools/generate_eevee.py",
        "# Local forward direction is +Z.",
        "",
    ]
    vertex_offset = 0
    normal_offset = 0
    for part in parts:
        lines.append(f"o {part.name}")
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.normals)
        for face in part.faces:
            indices = [
                f"{vertex_offset + index + 1}//{normal_offset + index + 1}"
                for index in face
            ]
            lines.append("f " + " ".join(indices))
        lines.append("")
        vertex_offset += len(part.vertices)
        normal_offset += len(part.normals)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    project_root = Path(__file__).resolve().parents[1]
    output = project_root / "resources" / "pokemon" / "eevee.obj"
    parts = build_parts()
    validate_parts(parts)
    write_obj(output, parts)
    triangles = sum(len(part.faces) for part in parts)
    print(f"Wrote {output.relative_to(project_root)}: {len(parts)} parts, {triangles} triangles")


if __name__ == "__main__":
    main()
