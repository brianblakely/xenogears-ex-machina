// Original repository-owned dependency fixture. This is not the content SDK.
import ManifoldModule from "manifold-3d";
import { BufferGeometry, Float32BufferAttribute } from "three";
import { Document, NodeIO } from "@gltf-transform/core";
import { z } from "zod";

function check(value: boolean, message: string): asserts value {
  if (!value) throw new Error(message);
}

const parameters = z.strictObject({ width: z.number().min(2).max(8) });
check(parameters.safeParse({ width: 4 }).success, "valid parameter rejected");
check(!parameters.safeParse({ width: 4, hidden: true }).success, "unknown parameter accepted");
check(!parameters.safeParse({ width: "4" }).success, "parameter silently coerced");
const schema = z.toJSONSchema(parameters, { target: "draft-2020-12" });
check(schema.additionalProperties === false, "schema lost closed-object rule");
let rejectedUnrepresentable = false;
try {
  z.toJSONSchema(z.date());
} catch {
  rejectedUnrepresentable = true;
}
check(rejectedUnrepresentable, "unrepresentable schema silently accepted");

const kernel = await ManifoldModule();
kernel.setup();
const wall = kernel.Manifold.cube([4, 3, 0.5], true);
const opening = kernel.Manifold.cube([1, 2, 1], true);
const cut = wall.subtract(opening);
try {
  const mesh = cut.getMesh();
  check(Math.abs(cut.volume() - 5) < 1e-6, "solid difference volume mismatch");
  check(mesh.numProp === 3 && mesh.triVerts.length > 0, "missing generated solid mesh");
  check(Array.from(mesh.vertProperties).every(Number.isFinite), "nonfinite solid vertices");

  // A deliberately open custom surface uses Three's buffer path, not Manifold.
  const surface = new BufferGeometry();
  try {
    surface.setAttribute("position", new Float32BufferAttribute([0, 0, 0, 2, 0, 0, 0, 0, -2], 3));
    surface.setIndex([0, 1, 2]);
    surface.computeVertexNormals();
    surface.computeBoundingBox();
    check(surface.getAttribute("normal").getY(0) === 1, "surface winding/normal mismatch");
    check(surface.boundingBox?.max.x === 2, "surface bounds mismatch");

    const doc = new Document();
    const buffer = doc.createBuffer("qualification-buffer");
    const positions = doc.createAccessor("source:wall/positions")
      .setType("VEC3").setArray(new Float32Array(mesh.vertProperties)).setBuffer(buffer);
    const indices = doc.createAccessor("source:wall/indices")
      .setType("SCALAR").setArray(new Uint32Array(mesh.triVerts)).setBuffer(buffer);
    const primitive = doc.createPrimitive().setAttribute("POSITION", positions).setIndices(indices);
    const renderMesh = doc.createMesh("source:wall").addPrimitive(primitive);
    doc.createScene("qualification").addChild(doc.createNode("source:wall-instance").setMesh(renderMesh));
    const io = new NodeIO();
    const first = await io.writeBinary(doc);
    const second = await io.writeBinary(doc);
    check(first.length === second.length && first.every((value, index) => value === second[index]),
      "same-input GLB serialization changed");
    const decoded = await io.readBinary(first);
    const decodedPrimitive = decoded.getRoot().listMeshes()[0]?.listPrimitives()[0];
    const decodedPositions = decodedPrimitive?.getAttribute("POSITION")?.getArray();
    const decodedIndices = decodedPrimitive?.getIndices()?.getArray();
    check(decodedPositions?.length === mesh.vertProperties.length, "GLB positions lost");
    check(decodedIndices?.length === mesh.triVerts.length, "GLB indices lost");
    check(decodedPositions.every((value, index) => value === mesh.vertProperties[index]),
      "GLB changed positions");
    check(decodedIndices.every((value, index) => value === mesh.triVerts[index]),
      "GLB changed indices");
    console.log(JSON.stringify({
      status: "passed",
      scope: "dependency_qualification_only",
      solidVolume: cut.volume(),
      solidVertices: mesh.vertProperties.length / mesh.numProp,
      solidTriangles: mesh.triVerts.length / 3,
      openSurfaceVertices: surface.getAttribute("position").count,
      glbBytes: first.length,
      glbRepeatedBytesEqual: true,
      schemaUnknownFieldsRejected: true,
      runtimeBridgeTested: false,
      sandboxTested: false,
    }));
  } finally {
    surface.dispose();
  }
} finally {
  cut.delete();
  opening.delete();
  wall.delete();
}
