#!/usr/bin/env python3
"""Generate the original low-poly Bulbasaur-inspired field model.

The mesh is deliberately built from simple primitives so its source and
animation groups remain editable in this repository.  It does not contain
geometry or textures extracted from a commercial Pokemon game.
"""

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


def add(a: Vec3, b: Vec3) -> Vec3:
    return a[0] + b[0], a[1] + b[1], a[2] + b[2]


def subtract(a: Vec3, b: Vec3) -> Vec3:
    return a[0] - b[0], a[1] - b[1], a[2] - b[2]


def multiply(value: Vec3, scale: Vec3) -> Vec3:
    return value[0] * scale[0], value[1] * scale[1], value[2] * scale[2]


def dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def normalize(value: Vec3) -> Vec3:
    length = sqrt(dot(value, value))
    if length <= 1.0e-8:
        return 0.0, 1.0, 0.0
    return value[0] / length, value[1] / length, value[2] / length


def rotate(value: Vec3, rotation: Vec3) -> Vec3:
    """Rotate in X, then Y, then Z order."""
    x, y, z = value
    cx, sx = cos(rotation[0]), sin(rotation[0])
    y, z = y * cx - z * sx, y * sx + z * cx
    cy, sy = cos(rotation[1]), sin(rotation[1])
    x, z = x * cy + z * sy, -x * sy + z * cy
    cz, sz = cos(rotation[2]), sin(rotation[2])
    x, y = x * cz - y * sz, x * sz + y * cz
    return x, y, z


def append_oriented_face(
    part: MeshPart, face: Face, surface_center: Vec3
) -> None:
    a, b, c = (part.vertices[index] for index in face)
    face_normal = cross(subtract(b, a), subtract(c, a))
    centroid = (
        (a[0] + b[0] + c[0]) / 3.0,
        (a[1] + b[1] + c[1]) / 3.0,
        (a[2] + b[2] + c[2]) / 3.0,
    )
    if dot(face_normal, subtract(centroid, surface_center)) < 0.0:
        part.faces.append((face[0], face[2], face[1]))
    else:
        part.faces.append(face)


def add_ellipsoid(
    part: MeshPart,
    center: Vec3,
    radii: Vec3,
    *,
    rotation: Vec3 = (0.0, 0.0, 0.0),
    rings: int = 5,
    segments: int = 10,
) -> None:
    if min(radii) <= 0.0:
        raise ValueError("ellipsoid radii must be positive")
    if rings < 3 or segments < 4:
        raise ValueError("ellipsoid tessellation is too small")

    def add_vertex(unit: Vec3) -> int:
        position = add(center, rotate(multiply(unit, radii), rotation))
        normal = normalize(
            rotate(
                (
                    unit[0] / radii[0],
                    unit[1] / radii[1],
                    unit[2] / radii[2],
                ),
                rotation,
            )
        )
        index = len(part.vertices)
        part.vertices.append(position)
        part.normals.append(normal)
        return index

    top = add_vertex((0.0, 1.0, 0.0))
    rows: list[list[int]] = []
    for ring in range(1, rings):
        theta = pi * ring / rings
        row: list[int] = []
        for segment in range(segments):
            phi = 2.0 * pi * segment / segments
            row.append(
                add_vertex(
                    (sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi))
                )
            )
        rows.append(row)
    bottom = add_vertex((0.0, -1.0, 0.0))

    for segment in range(segments):
        next_segment = (segment + 1) % segments
        append_oriented_face(
            part, (top, rows[0][segment], rows[0][next_segment]), center
        )
    for row_index in range(len(rows) - 1):
        upper = rows[row_index]
        lower = rows[row_index + 1]
        for segment in range(segments):
            next_segment = (segment + 1) % segments
            append_oriented_face(
                part,
                (upper[segment], lower[segment], lower[next_segment]),
                center,
            )
            append_oriented_face(
                part,
                (upper[segment], lower[next_segment], upper[next_segment]),
                center,
            )
    for segment in range(segments):
        next_segment = (segment + 1) % segments
        append_oriented_face(
            part,
            (rows[-1][segment], bottom, rows[-1][next_segment]),
            center,
        )


def add_pyramid(
    part: MeshPart,
    base_center: Vec3,
    tip: Vec3,
    half_width: float,
    half_depth: float,
) -> None:
    base = [
        add(base_center, (-half_width, 0.0, -half_depth)),
        add(base_center, (half_width, 0.0, -half_depth)),
        add(base_center, (half_width, 0.0, half_depth)),
        add(base_center, (-half_width, 0.0, half_depth)),
    ]
    interior = (
        (base_center[0] + tip[0]) * 0.5,
        (base_center[1] + tip[1]) * 0.5,
        (base_center[2] + tip[2]) * 0.5,
    )
    triangles = [
        (base[0], base[1], tip),
        (base[1], base[2], tip),
        (base[2], base[3], tip),
        (base[3], base[0], tip),
        (base[0], base[3], base[2]),
        (base[0], base[2], base[1]),
    ]
    for triangle in triangles:
        face_normal = normalize(
            cross(
                subtract(triangle[1], triangle[0]),
                subtract(triangle[2], triangle[0]),
            )
        )
        centroid = (
            sum(point[0] for point in triangle) / 3.0,
            sum(point[1] for point in triangle) / 3.0,
            sum(point[2] for point in triangle) / 3.0,
        )
        points = triangle
        if dot(face_normal, subtract(centroid, interior)) < 0.0:
            points = triangle[0], triangle[2], triangle[1]
            face_normal = (-face_normal[0], -face_normal[1], -face_normal[2])
        first = len(part.vertices)
        part.vertices.extend(points)
        part.normals.extend([face_normal, face_normal, face_normal])
        part.faces.append((first, first + 1, first + 2))


def build_parts() -> list[MeshPart]:
    parts: dict[str, MeshPart] = {}

    def part(name: str) -> MeshPart:
        return parts.setdefault(name, MeshPart(name))

    body = part("body")
    add_ellipsoid(body, (0.0, 0.43, -0.10), (0.42, 0.29, 0.50))
    add_ellipsoid(body, (0.0, 0.53, 0.39), (0.37, 0.33, 0.34))
    add_ellipsoid(body, (0.0, 0.42, 0.61), (0.29, 0.16, 0.16), rings=4)
    add_pyramid(body, (-0.22, 0.75, 0.39), (-0.30, 1.04, 0.39), 0.09, 0.09)
    add_pyramid(body, (0.22, 0.75, 0.39), (0.30, 1.04, 0.39), 0.09, 0.09)

    leg_positions = {
        "leg-front-left": (-0.27, 0.22, 0.29),
        "leg-front-right": (0.27, 0.22, 0.29),
        "leg-back-left": (-0.27, 0.22, -0.31),
        "leg-back-right": (0.27, 0.22, -0.31),
    }
    for name, center in leg_positions.items():
        leg = part(name)
        add_ellipsoid(leg, center, (0.13, 0.22, 0.14), rings=4, segments=8)
        foot_center = (center[0], 0.075, center[2] + 0.055)
        add_ellipsoid(leg, foot_center, (0.15, 0.075, 0.18), rings=4, segments=8)

    spots = part("spots")
    for center, radii, rotation in [
        ((-0.39, 0.47, -0.03), (0.018, 0.095, 0.12), (0.0, 0.0, -0.25)),
        ((0.39, 0.47, -0.03), (0.018, 0.095, 0.12), (0.0, 0.0, 0.25)),
        ((-0.33, 0.61, 0.36), (0.018, 0.07, 0.08), (0.0, 0.0, -0.25)),
        ((0.33, 0.61, 0.36), (0.018, 0.07, 0.08), (0.0, 0.0, 0.25)),
    ]:
        add_ellipsoid(spots, center, radii, rotation=rotation, rings=3, segments=7)

    bulb = part("bulb-main")
    add_ellipsoid(bulb, (0.0, 0.80, -0.20), (0.32, 0.32, 0.30), rings=5)

    leaves = part("bulb-leaves")
    for angle in (0.0, 0.5 * pi, pi, 1.5 * pi):
        center = (0.19 * sin(angle), 0.94, -0.20 + 0.19 * cos(angle))
        add_ellipsoid(
            leaves,
            center,
            (0.12, 0.075, 0.28),
            rotation=(0.0, angle, 0.0),
            rings=3,
            segments=7,
        )

    eye_white = part("eye-white")
    eye_iris = part("eye-iris")
    eye_dark = part("eye-dark")
    eye_highlight = part("eye-highlight")
    for side in (-1.0, 1.0):
        add_ellipsoid(
            eye_white, (0.17 * side, 0.58, 0.695), (0.105, 0.13, 0.022), rings=4, segments=8
        )
        add_ellipsoid(
            eye_iris, (0.17 * side, 0.57, 0.718), (0.060, 0.085, 0.014), rings=4, segments=8
        )
        add_ellipsoid(
            eye_dark, (0.17 * side, 0.565, 0.733), (0.027, 0.052, 0.009), rings=3, segments=7
        )
        add_ellipsoid(
            eye_highlight,
            (0.153 * side, 0.600, 0.742),
            (0.012, 0.018, 0.006),
            rings=3,
            segments=6,
        )
    add_ellipsoid(eye_dark, (-0.075, 0.43, 0.764), (0.016, 0.010, 0.006), rings=3, segments=6)
    add_ellipsoid(eye_dark, (0.075, 0.43, 0.764), (0.016, 0.010, 0.006), rings=3, segments=6)
    add_ellipsoid(eye_dark, (0.0, 0.365, 0.765), (0.105, 0.012, 0.006), rings=3, segments=8)

    return list(parts.values())


def validate_parts(parts: list[MeshPart]) -> None:
    expected_names = {
        "body",
        "leg-front-left",
        "leg-front-right",
        "leg-back-left",
        "leg-back-right",
        "spots",
        "bulb-main",
        "bulb-leaves",
        "eye-white",
        "eye-iris",
        "eye-dark",
        "eye-highlight",
    }
    names = {part.name for part in parts}
    if names != expected_names:
        raise ValueError(f"unexpected animation groups: {sorted(names)}")
    for part in parts:
        if not part.vertices or not part.faces:
            raise ValueError(f"empty mesh group: {part.name}")
        if len(part.vertices) != len(part.normals):
            raise ValueError(f"vertex/normal mismatch in {part.name}")
        for face in part.faces:
            if any(index < 0 or index >= len(part.vertices) for index in face):
                raise ValueError(f"invalid face index in {part.name}")
        for normal in part.normals:
            if abs(dot(normal, normal) - 1.0) > 1.0e-4:
                raise ValueError(f"non-unit normal in {part.name}")


def write_obj(path: Path, parts: list[MeshPart]) -> None:
    lines = [
        "# Pokemon World original procedural low-poly creature",
        "# Regenerate with: python3 tools/generate_bulbasaur.py",
        "# Local forward direction is +Z.",
        "",
    ]
    vertex_offset = 0
    normal_offset = 0
    for part in parts:
        lines.append(f"o {part.name}")
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in part.normals)
        for a, b, c in part.faces:
            indices = []
            for index in (a, b, c):
                vertex_index = vertex_offset + index + 1
                normal_index = normal_offset + index + 1
                indices.append(f"{vertex_index}//{normal_index}")
            lines.append("f " + " ".join(indices))
        lines.append("")
        vertex_offset += len(part.vertices)
        normal_offset += len(part.normals)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    project_root = Path(__file__).resolve().parents[1]
    output = project_root / "resources" / "pokemon" / "bulbasaur.obj"
    parts = build_parts()
    validate_parts(parts)
    write_obj(output, parts)
    triangle_count = sum(len(part.faces) for part in parts)
    print(f"Wrote {output.relative_to(project_root)}: {len(parts)} parts, {triangle_count} triangles")


if __name__ == "__main__":
    main()
