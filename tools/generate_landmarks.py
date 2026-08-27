#!/usr/bin/env python3
"""Generate the original low-poly field landmarks used by Pokemon World."""

from __future__ import annotations

from dataclasses import dataclass, field
from math import cos, pi, sin, sqrt
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
        raise ValueError("landmark triangle is degenerate")
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


def ring(radius: float, height: float, sides: int,
         phase: float = 0.0) -> list[Vec3]:
    return [
        (
            cos(phase + side * 2.0 * pi / sides) * radius,
            height,
            sin(phase + side * 2.0 * pi / sides) * radius,
        )
        for side in range(sides)
    ]


def add_cylinder(part: MeshPart, radius: float, bottom: float, top: float,
                 sides: int) -> None:
    lower = ring(radius, bottom, sides, pi / sides)
    upper = ring(radius * 0.88, top, sides, pi / sides)
    for side in range(sides):
        next_side = (side + 1) % sides
        add_quad(part, lower[side], upper[side], upper[next_side],
                 lower[next_side])
        add_triangle(part, (0.0, bottom, 0.0), lower[next_side], lower[side])
        add_triangle(part, (0.0, top, 0.0), upper[side], upper[next_side])


def add_cone(part: MeshPart, radius: float, base: float, apex: float,
             sides: int, phase: float = 0.0) -> None:
    base_ring = ring(radius, base, sides, phase)
    tip = (0.0, apex, 0.0)
    for side in range(sides):
        next_side = (side + 1) % sides
        add_triangle(part, base_ring[side], tip, base_ring[next_side])
        add_triangle(part, (0.0, base, 0.0), base_ring[next_side],
                     base_ring[side])


def add_tapered_spire(part: MeshPart) -> None:
    sides = 7
    lower = ring(1.45, 0.0, sides, 0.16)
    upper = ring(0.48, 4.75, sides, 0.48)
    for side in range(sides):
        lower_scale = 0.88 + 0.15 * sin(side * 2.17)
        upper_scale = 0.82 + 0.12 * cos(side * 1.73)
        lower[side] = (
            lower[side][0] * lower_scale,
            lower[side][1],
            lower[side][2] * lower_scale,
        )
        upper[side] = (
            upper[side][0] * upper_scale,
            upper[side][1] + 0.18 * sin(side * 1.31),
            upper[side][2] * upper_scale,
        )
    for side in range(sides):
        next_side = (side + 1) % sides
        add_quad(part, lower[side], upper[side], upper[next_side],
                 lower[next_side])
        add_triangle(part, (0.0, 0.0, 0.0), lower[next_side], lower[side])


def add_crystal(part: MeshPart) -> None:
    sides = 6
    middle = ring(0.58, 5.05, sides, pi / 6.0)
    bottom = (0.0, 4.40, 0.0)
    top = (0.0, 7.15, 0.0)
    for side in range(sides):
        next_side = (side + 1) % sides
        add_triangle(part, middle[side], top, middle[next_side])
        add_triangle(part, middle[side], middle[next_side], bottom)


def build_parts() -> list[MeshPart]:
    trunk = MeshPart("moon-tree-trunk")
    add_cylinder(trunk, 0.34, 0.0, 2.05, 7)

    lower_canopy = MeshPart("moon-tree-canopy-low")
    add_cone(lower_canopy, 1.75, 1.15, 3.65, 8, pi / 8.0)

    upper_canopy = MeshPart("moon-tree-canopy-high")
    add_cone(upper_canopy, 1.28, 2.35, 4.75, 8)

    red_rock = MeshPart("red-spire-rock")
    add_tapered_spire(red_rock)

    crystal = MeshPart("red-spire-crystal")
    add_crystal(crystal)

    return [trunk, lower_canopy, upper_canopy, red_rock, crystal]


def validate_parts(parts: list[MeshPart]) -> None:
    expected = {
        "moon-tree-trunk",
        "moon-tree-canopy-low",
        "moon-tree-canopy-high",
        "red-spire-rock",
        "red-spire-crystal",
    }
    if {part.name for part in parts} != expected:
        raise ValueError("landmark mesh groups do not match the runtime contract")
    for part in parts:
        if not part.vertices or not part.faces:
            raise ValueError(f"empty landmark mesh group: {part.name}")
        if len(part.vertices) != len(part.normals):
            raise ValueError(f"vertex/normal mismatch in {part.name}")


def write_obj(path: Path, parts: list[MeshPart]) -> None:
    lines = [
        "# Pokemon World original procedural low-poly field landmarks",
        "# Regenerate with: python3 tools/generate_landmarks.py",
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
            encoded = [
                f"{vertex_offset + index + 1}//{normal_offset + index + 1}"
                for index in face
            ]
            lines.append("f " + " ".join(encoded))
        lines.append("")
        vertex_offset += len(part.vertices)
        normal_offset += len(part.normals)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    project_root = Path(__file__).resolve().parents[1]
    output = project_root / "resources" / "world" / "field_landmarks.obj"
    parts = build_parts()
    validate_parts(parts)
    write_obj(output, parts)
    triangles = sum(len(part.faces) for part in parts)
    print(
        f"Wrote {output.relative_to(project_root)}: "
        f"{len(parts)} parts, {triangles} triangles"
    )


if __name__ == "__main__":
    main()
