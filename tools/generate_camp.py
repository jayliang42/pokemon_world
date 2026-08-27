#!/usr/bin/env python3
"""Generate the original low-poly field camp used by Pokemon World."""

from __future__ import annotations

from dataclasses import dataclass, field
from math import sqrt
from pathlib import Path


Vec3 = tuple[float, float, float]
Face = tuple[int, int, int]


@dataclass
class MeshPart:
    name: str
    vertices: list[Vec3] = field(default_factory=list)
    normals: list[Vec3] = field(default_factory=list)
    faces: list[Face] = field(default_factory=list)


def subtract(a: Vec3, b: Vec3) -> Vec3:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(value: Vec3) -> Vec3:
    length = sqrt(sum(component * component for component in value))
    if length <= 1.0e-8:
        raise ValueError("camp triangle is degenerate")
    return value[0] / length, value[1] / length, value[2] / length


def add_triangle(part: MeshPart, a: Vec3, b: Vec3, c: Vec3) -> None:
    normal = normalize(cross(subtract(b, a), subtract(c, a)))
    start = len(part.vertices)
    part.vertices.extend((a, b, c))
    part.normals.extend((normal, normal, normal))
    part.faces.append((start, start + 1, start + 2))


def add_quad(part: MeshPart, a: Vec3, b: Vec3, c: Vec3, d: Vec3) -> None:
    add_triangle(part, a, b, c)
    add_triangle(part, a, c, d)


def add_box(part: MeshPart, minimum: Vec3, maximum: Vec3) -> None:
    x0, y0, z0 = minimum
    x1, y1, z1 = maximum
    add_quad(part, (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))
    add_quad(part, (x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0))
    add_quad(part, (x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1))
    add_quad(part, (x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0))
    add_quad(part, (x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0))
    add_quad(part, (x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1))


def add_tent(part: MeshPart) -> None:
    left_front = (-6.1, 0.0, 3.0)
    right_front = (-1.9, 0.0, 3.0)
    ridge_front = (-4.0, 2.7, 3.0)
    left_back = (-6.1, 0.0, 0.0)
    right_back = (-1.9, 0.0, 0.0)
    ridge_back = (-4.0, 2.7, 0.0)
    add_triangle(part, left_front, right_front, ridge_front)
    add_triangle(part, right_back, left_back, ridge_back)
    add_quad(part, left_back, left_front, ridge_front, ridge_back)
    add_quad(part, right_front, right_back, ridge_back, ridge_front)
    add_quad(part, left_front, left_back, right_back, right_front)


def build_parts() -> list[MeshPart]:
    tent = MeshPart("tent-fabric")
    add_tent(tent)

    entrance = MeshPart("tent-entrance")
    add_triangle(entrance, (-5.0, 0.03, 3.02), (-3.0, 0.03, 3.02),
                 (-4.0, 1.75, 3.02))

    workbench = MeshPart("workbench")
    add_box(workbench, (2.0, 0.82, -2.1), (5.0, 1.03, -0.9))
    for x0, x1 in ((2.15, 2.42), (4.58, 4.85)):
        add_box(workbench, (x0, 0.0, -1.95), (x1, 0.82, -1.68))
        add_box(workbench, (x0, 0.0, -1.32), (x1, 0.82, -1.05))

    crate = MeshPart("supply-crate")
    add_box(crate, (2.75, 0.0, 0.85), (4.25, 1.15, 2.15))
    add_box(crate, (2.65, 1.15, 0.76), (4.35, 1.34, 2.24))

    flagpole = MeshPart("flagpole")
    add_box(flagpole, (-0.62, 0.0, -3.92), (-0.42, 4.0, -3.72))

    flag = MeshPart("camp-flag")
    add_quad(flag, (-0.42, 3.85, -3.82), (1.55, 3.58, -3.82),
             (1.55, 2.65, -3.82), (-0.42, 2.92, -3.82))

    return [tent, entrance, workbench, crate, flagpole, flag]


def validate_parts(parts: list[MeshPart]) -> None:
    expected = {"tent-fabric", "tent-entrance", "workbench", "supply-crate",
                "flagpole", "camp-flag"}
    if {part.name for part in parts} != expected:
        raise ValueError("camp mesh groups do not match the runtime contract")
    for part in parts:
        if not part.vertices or not part.faces:
            raise ValueError(f"empty camp mesh group: {part.name}")
        if len(part.vertices) != len(part.normals):
            raise ValueError(f"vertex/normal mismatch in {part.name}")


def write_obj(path: Path, parts: list[MeshPart]) -> None:
    lines = [
        "# Pokemon World original procedural low-poly field camp",
        "# Regenerate with: python3 tools/generate_camp.py",
        "# Coordinates are authored directly in world-scale meters.",
        "",
    ]
    vertex_offset = 0
    normal_offset = 0
    for part in parts:
        lines.append(f"o {part.name}")
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.normals)
        for face in part.faces:
            encoded = []
            for index in face:
                encoded.append(
                    f"{vertex_offset + index + 1}//{normal_offset + index + 1}"
                )
            lines.append("f " + " ".join(encoded))
        lines.append("")
        vertex_offset += len(part.vertices)
        normal_offset += len(part.normals)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    project_root = Path(__file__).resolve().parents[1]
    output = project_root / "resources" / "camp" / "field_camp.obj"
    parts = build_parts()
    validate_parts(parts)
    write_obj(output, parts)
    triangles = sum(len(part.faces) for part in parts)
    print(f"Wrote {output.relative_to(project_root)}: {len(parts)} parts, {triangles} triangles")


if __name__ == "__main__":
    main()
