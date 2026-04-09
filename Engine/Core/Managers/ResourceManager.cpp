#include "ResourceManager.h"
#include "../../Renderer/Texture.h"
#include "../../Renderer/VulkanRHI.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

const float PI = 3.14159265358979323846f;
static Mesh::Vertex MakeVertex(float px, float py, float pz,
    float nx, float ny, float nz,
    float u = 0.0f, float v = 0.0f)
{
    Mesh::Vertex vert{};
    vert.position[0] = px; vert.position[1] = py; vert.position[2] = pz;
    vert.normal[0] = nx; vert.normal[1] = ny; vert.normal[2] = nz;
    vert.uv[0] = u; vert.uv[1] = v;
    return vert;
}

MeshData ResourceManager::Create2dTriangleMesh()
{
    std::vector<Mesh::Vertex> verts(3);
    verts[0].position[0] = 0.0f; verts[0].position[1] = 0.5f; verts[0].position[2] = 0.0f;
    verts[1].position[0] = 0.5f; verts[1].position[1] = -0.5f; verts[1].position[2] = 0.0f;
    verts[2].position[0] = -0.5f; verts[2].position[1] = -0.5f; verts[2].position[2] = 0.0f;

    // Improve lighting by giving a normal pointing out of the screen
    // and give each vertex sensible UVs. Increase `uvTile` to tile the
    // brick texture across the triangle (makes bricks appear smaller / clearer).
    const float uvTile = 1.0f; // tweak this (e.g. 2..8) to change brick density/clarity

    // top vertex
    verts[0].normal[0] = 0.0f; verts[0].normal[1] = 0.0f; verts[0].normal[2] = 1.0f;
    verts[0].uv[0] = 0.5f * uvTile; verts[0].uv[1] = 1.0f * uvTile;

    // bottom-right
    verts[1].normal[0] = 0.0f; verts[1].normal[1] = 0.0f; verts[1].normal[2] = 1.0f;
    verts[1].uv[0] = 1.0f * uvTile; verts[1].uv[1] = 0.0f * uvTile;

    // bottom-left
    verts[2].normal[0] = 0.0f; verts[2].normal[1] = 0.0f; verts[2].normal[2] = 1.0f;
    verts[2].uv[0] = 0.0f * uvTile; verts[2].uv[1] = 0.0f * uvTile;

    std::vector<uint32_t> indices = { 0, 1, 2 };
    return { verts, indices };
}
MeshData ResourceManager::CreateQuadMesh()
{
    // unit quad centered at origin (XY plane, +Z normal)
    std::vector<Mesh::Vertex> verts(4);
    verts[0] = MakeVertex(-0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f); // top-left
    verts[1] = MakeVertex(0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); // top-right
    verts[2] = MakeVertex(0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f); // bottom-right
    verts[3] = MakeVertex(-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f); // bottom-left

    std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };
    return { verts, indices };
}
MeshData ResourceManager::CreateCubeMesh()
{
    // Unit cube centered at origin extents [-0.5,0.5], 24 vertices (4 per face)
    std::vector<Mesh::Vertex> verts(24);
    const float s = 0.5f;

    // Front (+Z)
    verts[0] = MakeVertex(-s, s, s, 0, 0, 1, 0, 1);
    verts[1] = MakeVertex(s, s, s, 0, 0, 1, 1, 1);
    verts[2] = MakeVertex(s, -s, s, 0, 0, 1, 1, 0);
    verts[3] = MakeVertex(-s, -s, s, 0, 0, 1, 0, 0);

    // Back (-Z)
    verts[4] = MakeVertex(s, s, -s, 0, 0, -1, 0, 1);
    verts[5] = MakeVertex(-s, s, -s, 0, 0, -1, 1, 1);
    verts[6] = MakeVertex(-s, -s, -s, 0, 0, -1, 1, 0);
    verts[7] = MakeVertex(s, -s, -s, 0, 0, -1, 0, 0);

    // Right (+X)
    verts[8] = MakeVertex(s, s, s, 1, 0, 0, 0, 1);
    verts[9] = MakeVertex(s, s, -s, 1, 0, 0, 1, 1);
    verts[10] = MakeVertex(s, -s, -s, 1, 0, 0, 1, 0);
    verts[11] = MakeVertex(s, -s, s, 1, 0, 0, 0, 0);

    // Left (-X)
    verts[12] = MakeVertex(-s, s, -s, -1, 0, 0, 0, 1);
    verts[13] = MakeVertex(-s, s, s, -1, 0, 0, 1, 1);
    verts[14] = MakeVertex(-s, -s, s, -1, 0, 0, 1, 0);
    verts[15] = MakeVertex(-s, -s, -s, -1, 0, 0, 0, 0);

    // Top (+Y)
    verts[16] = MakeVertex(-s, s, -s, 0, 1, 0, 0, 1);
    verts[17] = MakeVertex(s, s, -s, 0, 1, 0, 1, 1);
    verts[18] = MakeVertex(s, s, s, 0, 1, 0, 1, 0);
    verts[19] = MakeVertex(-s, s, s, 0, 1, 0, 0, 0);

    // Bottom (-Y)
    verts[20] = MakeVertex(-s, -s, s, 0, -1, 0, 0, 1);
    verts[21] = MakeVertex(s, -s, s, 0, -1, 0, 1, 1);
    verts[22] = MakeVertex(s, -s, -s, 0, -1, 0, 1, 0);
    verts[23] = MakeVertex(-s, -s, -s, 0, -1, 0, 0, 0);

    std::vector<uint32_t> indices;
    indices.reserve(36);
    for (uint32_t face = 0; face < 6; ++face)
    {
        uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    return { verts, indices };
}

MeshData ResourceManager::CreateSphereMesh(float uv, uint32_t sectorCount, uint32_t stackCount)
{
    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> indices;

    float radius = 0.5f;

    // create vertices
    for (uint32_t i = 0; i <= stackCount; ++i)
    {
        float stackAngle = PI / 2.0f - static_cast<float>(i) * PI / static_cast<float>(stackCount); // from pi/2 to -pi/2
        float xy = radius * std::cos(stackAngle);
        float z = radius * std::sin(stackAngle);

        for (uint32_t j = 0; j <= sectorCount; ++j)
        {
            float sectorAngle = static_cast<float>(j) * 2.0f * PI / static_cast<float>(sectorCount);
            float x = xy * std::cos(sectorAngle);
            float y = xy * std::sin(sectorAngle);

            // normal is just normalized position for unit sphere
            float nx = x / radius;
            float ny = y / radius;
            float nz = z / radius;

            float u = static_cast<float>(j) / static_cast<float>(sectorCount);
            float v = static_cast<float>(i) / static_cast<float>(stackCount);

            // apply uv tiling
            u *= uv;
            v *= uv;

            verts.push_back(MakeVertex(x, y, z, nx, ny, nz, u, v));
        }
    }

    // create indices
    for (uint32_t i = 0; i < stackCount; ++i)
    {
        uint32_t k1 = i * (sectorCount + 1);
        uint32_t k2 = k1 + sectorCount + 1;

        for (uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    return { verts, indices };
}
MeshData ResourceManager::CreatePlaneMesh(float uv, float width, float height, uint32_t widthSegments, uint32_t heightSegments)
{
    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> indices;

    // guard against non-positive dimensions
    if (width <= 0.0f || height <= 0.0f)
        return { verts, indices };

    uint32_t wSeg = std::max<uint32_t>(1u, widthSegments);
    uint32_t hSeg = std::max<uint32_t>(1u, heightSegments);

    // reserve to avoid reallocations
    verts.reserve(static_cast<size_t>(wSeg + 1) * static_cast<size_t>(hSeg + 1));
    indices.reserve(static_cast<size_t>(wSeg) * static_cast<size_t>(hSeg) * 6u);

    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    // vertices (row-major: y then x)
    for (uint32_t y = 0; y <= hSeg; ++y)
    {
        float v = static_cast<float>(y) / static_cast<float>(hSeg);
        float py = halfH - v * height;
        for (uint32_t x = 0; x <= wSeg; ++x)
        {
            float u = static_cast<float>(x) / static_cast<float>(wSeg);
            float px = -halfW + u * width;

            float tu = u * uv;
            float tv = (1.0f - v) * uv; // keep same vertical orientation but apply tiling

            verts.push_back(MakeVertex(px, py, 0.0f, 0.0f, 0.0f, 1.0f, tu, tv));
        }
    }

    // indices (two triangles per quad)
    for (uint32_t y = 0; y < hSeg; ++y)
    {
        for (uint32_t x = 0; x < wSeg; ++x)
        {
            uint32_t a = y * (wSeg + 1) + x;           // top-left
            uint32_t b = a + (wSeg + 1);               // bottom-left
            uint32_t tl = a;
            uint32_t bl = b;
            uint32_t tr = a + 1;
            uint32_t br = b + 1;

            // triangle 1: top-left, bottom-left, top-right  (CCW for +Z)
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            // triangle 2: top-right, bottom-left, bottom-right (CCW for +Z)
            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }
    return { verts, indices };
}
MeshData ResourceManager::CreateCylinderMesh(float uv, float radius, float height, uint32_t sectorCount)
{
    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> indices;

    uint32_t sectors = std::max<uint32_t>(3, sectorCount);
    float halfH = height * 0.5f;

    // Choose cylinder v-range so it stitches consistently with the capsule hemisphere
    // (matches the v mapping used by CreateCapsuleMesh: top hemisphere equator at v=0.75,
    // cylinder occupies the middle band). These values (0.75 / 0.25) produce a continuous
    // vertical tiling appearance between hemisphere and cylinder.
    const float vTop = 0.75f;
    const float vBottom = 0.25f;

    // side vertices (two rings)
    for (uint32_t i = 0; i <= sectors; ++i)
    {
        float theta = static_cast<float>(i) * 2.0f * PI / static_cast<float>(sectors);
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        float nx = std::cos(theta);
        float nz = std::sin(theta);
        float u = static_cast<float>(i) / static_cast<float>(sectors);

        // apply uv tiling for side
        float tu = u * uv;

        // top ring (use vTop)
        verts.push_back(MakeVertex(x, halfH, z, nx, 0.0f, nz, tu, vTop * uv));
        // bottom ring (use vBottom)
        verts.push_back(MakeVertex(x, -halfH, z, nx, 0.0f, nz, tu, vBottom * uv));
    }

    // side indices
    // each sector uses two vertices per ring -> 2 * (sectors + 1) verts created above
    for (uint32_t i = 0; i < sectors; ++i)
    {
        uint32_t top1 = i * 2;
        uint32_t bottom1 = top1 + 1;
        uint32_t top2 = (i + 1) * 2;
        uint32_t bottom2 = top2 + 1;

        indices.push_back(top1);
        indices.push_back(bottom1);
        indices.push_back(top2);

        indices.push_back(top2);
        indices.push_back(bottom1);
        indices.push_back(bottom2);
    }

    // caps (fan)
    // top center
    uint32_t topCenterIndex = static_cast<uint32_t>(verts.size());
    verts.push_back(MakeVertex(0.0f, halfH, 0.0f, 0, 1, 0, 0.5f * uv, 0.5f * uv));

    for (uint32_t i = 0; i < sectors; ++i)
    {
        float theta = static_cast<float>(i) * 2.0f * PI / static_cast<float>(sectors);
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        verts.push_back(MakeVertex(x, halfH, z, 0, 1, 0, (std::cos(theta) + 1.0f) * 0.5f * uv, (std::sin(theta) + 1.0f) * 0.5f * uv));
    }
    // build top fan
    for (uint32_t i = 0; i < sectors; ++i)
    {
        uint32_t a = topCenterIndex;
        uint32_t b = topCenterIndex + 1 + i;
        uint32_t c = topCenterIndex + 1 + ((i + 1) % sectors);
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    // bottom center
    uint32_t bottomCenterIndex = static_cast<uint32_t>(verts.size());
    verts.push_back(MakeVertex(0.0f, -halfH, 0.0f, 0, -1, 0, 0.5f * uv, 0.5f * uv));
    for (uint32_t i = 0; i < sectors; ++i)
    {
        float theta = static_cast<float>(i) * 2.0f * PI / static_cast<float>(sectors);
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        verts.push_back(MakeVertex(x, -halfH, z, 0, -1, 0, (std::cos(theta) + 1.0f) * 0.5f * uv, (std::sin(theta) + 1.0f) * 0.5f * uv));
    }
    // build bottom fan (note winding reversed to face outward)
    for (uint32_t i = 0; i < sectors; ++i)
    {
        uint32_t a = bottomCenterIndex;
        uint32_t b = bottomCenterIndex + 1 + ((i + 1) % sectors);
        uint32_t c = bottomCenterIndex + 1 + i;
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }

    return { verts, indices };
}
MeshData ResourceManager::CreateCapsuleMesh(float uv, float radius, float height, uint32_t sectorCount, uint32_t stackCount)
{
    // Capsule: cylinder of (height - 2*radius) with hemispheres top and bottom.
    std::vector<Mesh::Vertex> verts;
    std::vector<uint32_t> indices;

    uint32_t sectors = std::max<uint32_t>(3, sectorCount);
    uint32_t stacks = std::max<uint32_t>(2, stackCount);

    float cylHeight = height - 2.0f * radius;
    if (cylHeight < 0.0f) cylHeight = 0.0f;
    float halfCyl = cylHeight * 0.5f;

    // Generate rings for hemisphere (top) including equator (stackHalf segments)
    uint32_t halfStacks = std::max<uint32_t>(1, stacks / 2);

    // helper to add hemisphere (0..halfStacks) for top, offsetY should be +halfCyl
    auto buildHemisphere = [&](bool top, float offsetY)
    {
        // stack from 0..halfStacks (0 = pole, halfStacks = equator)
        for (uint32_t i = 0; i <= halfStacks; ++i)
        {
            float stackAngle = (static_cast<float>(i) / static_cast<float>(halfStacks)) * (PI * 0.5f); // 0..pi/2
            float sinS = std::sin(stackAngle);
            float cosS = std::cos(stackAngle);
            float y = radius * cosS; // distance above/below equator
            float ringRadius = radius * sinS;

            float py = (top ? 1.0f : -1.0f) * y + offsetY;

            for (uint32_t j = 0; j <= sectors; ++j)
            {
                float sectorAngle = static_cast<float>(j) * 2.0f * PI / static_cast<float>(sectors);
                float x = ringRadius * std::cos(sectorAngle);
                float z = ringRadius * std::sin(sectorAngle);

                // normal is vector from sphere center of hemisphere
                float nx = x / radius;
                float ny = (top ? 1.0f : -1.0f) * (y / radius);
                float nz = z / radius;

                float u = static_cast<float>(j) / static_cast<float>(sectors);

                // Adjust vertical UVs so hemisphere equator stitches to cylinder v-range [0.75, 0.25].
                // Top hemisphere: pole v=1.0 -> equator v=0.75
                // Bottom hemisphere (if built via this helper) would be mapped similarly.
                float v;
                if (top)
                    v = 1.0f - (static_cast<float>(i) / static_cast<float>(halfStacks)) * 0.25f; // 1.0 -> 0.75
                else
                    v = 0.25f - (static_cast<float>(halfStacks - i) / static_cast<float>(halfStacks)) * 0.25f; // 0.25 -> 0.0

                // apply tiling
                float tu = u * uv;
                float tv = v * uv;

                verts.push_back(MakeVertex(x, py, z, nx, ny, nz, tu, tv));
            }
        }
    };

    // top hemisphere centered at +halfCyl
    buildHemisphere(true, halfCyl);
    // remember index where equator of top hemisphere begins (last ring)
    uint32_t vertsPerRing = sectors + 1;
    uint32_t topHemRings = halfStacks + 1;
    uint32_t topEquatorStart = static_cast<uint32_t>(verts.size()) - vertsPerRing;

    // cylinder rings: top and bottom (always add them as distinct rings so normals are correct)
    for (uint32_t i = 0; i <= 1; ++i)
    {
        float py = (i == 0) ? halfCyl : -halfCyl; // top ring at +halfCyl, bottom at -halfCyl
        float v = (i == 0) ? 0.75f : 0.25f; // cylinder occupies middle band [0.75 .. 0.25]
        for (uint32_t j = 0; j <= sectors; ++j)
        {
            float sectorAngle = static_cast<float>(j) * 2.0f * PI / static_cast<float>(sectors);
            float x = radius * std::cos(sectorAngle);
            float z = radius * std::sin(sectorAngle);
            float nx = std::cos(sectorAngle);
            float nz = std::sin(sectorAngle);
            float u = static_cast<float>(j) / static_cast<float>(sectors);

            float tu = u * uv;
            float tv = v * uv;

            verts.push_back(MakeVertex(x, py, z, nx, 0.0f, nz, tu, tv));
        }
    }
    // IMPORTANT FIX:
    // cylinder top ring start is the first of the two rings we just appended.
    // compute top and bottom ring starts based on vertsPerRing and current verts size.
    uint32_t cylBottomStart = static_cast<uint32_t>(verts.size()) - vertsPerRing;
    uint32_t cylTopStart = cylBottomStart - vertsPerRing;

    // bottom hemisphere (build equator -> pole to make indexing symmetrical)
    uint32_t bottomHemStart = static_cast<uint32_t>(verts.size());
    for (int i = static_cast<int>(halfStacks); i >= 0; --i)
    {
        float stackAngle = (static_cast<float>(i) / static_cast<float>(halfStacks)) * (PI * 0.5f); // 0..pi/2
        float sinS = std::sin(stackAngle);
        float cosS = std::cos(stackAngle);
        float y = radius * cosS;
        float ringRadius = radius * sinS;

        float py = -y - halfCyl; // offsetY = -halfCyl for bottom

        for (uint32_t j = 0; j <= sectors; ++j)
        {
            float sectorAngle = static_cast<float>(j) * 2.0f * PI / static_cast<float>(sectors);
            float x = ringRadius * std::cos(sectorAngle);
            float z = ringRadius * std::sin(sectorAngle);

            float nx = x / radius;
            float ny = - (y / radius);
            float nz = z / radius;

            float u = static_cast<float>(j) / static_cast<float>(sectors);

            // Map bottom hemisphere v so equator = 0.25 and pole = 0.0
            float v = 0.25f - (static_cast<float>(halfStacks - i) / static_cast<float>(halfStacks)) * 0.25f; // 0.25 -> 0.0

            float tu = u * uv;
            float tv = v * uv;

            verts.push_back(MakeVertex(x, py, z, nx, ny, nz, tu, tv));
        }
    }
    uint32_t bottomHemRings = halfStacks + 1;
    uint32_t bottomHemEquatorStart = bottomHemStart; // we built bottom hemisphere from equator down

    // Build indices for top hemisphere (pole -> equator)
    uint32_t startTopHem = 0;
    for (uint32_t i = 0; i < topHemRings - 1; ++i)
    {
        uint32_t ringStart = startTopHem + i * vertsPerRing;
        uint32_t nextRing = ringStart + vertsPerRing;
        for (uint32_t j = 0; j < sectors; ++j)
        {
            if (i == 0)
            {
                // triangle from pole
                indices.push_back(ringStart);
                indices.push_back(nextRing + j);
                indices.push_back(nextRing + j + 1);
            }
            else
            {
                indices.push_back(ringStart + j);
                indices.push_back(nextRing + j);
                indices.push_back(ringStart + j + 1);

                indices.push_back(ringStart + j + 1);
                indices.push_back(nextRing + j);
                indices.push_back(nextRing + j + 1);
            }
        }
    }

    // connect top hemisphere equator to cylinder top ring
    for (uint32_t j = 0; j < sectors; ++j)
    {
        indices.push_back(topEquatorStart + j);
        indices.push_back(cylTopStart + j);
        indices.push_back(topEquatorStart + j + 1);

        indices.push_back(topEquatorStart + j + 1);
        indices.push_back(cylTopStart + j);
        indices.push_back(cylTopStart + j + 1);
    }

    // Build side quads between cylinder top ring and cylinder bottom ring
    for (uint32_t j = 0; j < sectors; ++j)
    {
        uint32_t topA = cylTopStart + j;
        uint32_t topB = cylTopStart + j + 1;
        uint32_t bottomA = cylBottomStart + j;
        uint32_t bottomB = cylBottomStart + j + 1;

        indices.push_back(topA);
        indices.push_back(bottomA);
        indices.push_back(topB);

        indices.push_back(topB);
        indices.push_back(bottomA);
        indices.push_back(bottomB);
    }

    // connect cylinder bottom ring to bottom hemisphere equator
    for (uint32_t j = 0; j < sectors; ++j)
    {
        indices.push_back(cylBottomStart + j);
        indices.push_back(bottomHemEquatorStart + j + 1);
        indices.push_back(cylBottomStart + j + 1);

        indices.push_back(cylBottomStart + j);
        indices.push_back(bottomHemEquatorStart + j);
        indices.push_back(bottomHemEquatorStart + j + 1);
    }

    // bottom hemisphere indices (equator -> pole)
    uint32_t startBottomHem = bottomHemStart;
    for (uint32_t i = 0; i < bottomHemRings - 1; ++i)
    {
        uint32_t ringStart = startBottomHem + i * vertsPerRing;
        uint32_t nextRing = ringStart + vertsPerRing;
        for (uint32_t j = 0; j < sectors; ++j)
        {
            if (i == bottomHemRings - 2)
            {
                // last ring before pole -> triangles into pole
                indices.push_back(ringStart + j);
                indices.push_back(nextRing + j + 1);
                indices.push_back(ringStart + j + 1);
            }
            else
            {
                indices.push_back(ringStart + j);
                indices.push_back(nextRing + j);
                indices.push_back(ringStart + j + 1);

                indices.push_back(ringStart + j + 1);
                indices.push_back(nextRing + j);
                indices.push_back(nextRing + j + 1);
            }
        }
    }

    // Note: The above implementation aims for a reasonable capsule topology:
    // - top hemisphere
    // - cylinder (two rings)
    // - bottom hemisphere
    // Winding / index connectivity are consistent for outward-facing triangles.
    // This is a moderate-quality capsule mesh suitable for general use.

    return { verts, indices };
}

std::shared_ptr<Texture> Create1x1Texture(VulkanRHI* rhi, const glm::vec4& color, TextureType type, bool srgb)
{
    if (!rhi) return nullptr;

    // Clamp and convert floats [0,1] to bytes [0,255]
    auto clamp01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
    unsigned char px[4];
    px[0] = static_cast<unsigned char>(std::round(clamp01(color.r) * 255.0f));
    px[1] = static_cast<unsigned char>(std::round(clamp01(color.g) * 255.0f));
    px[2] = static_cast<unsigned char>(std::round(clamp01(color.b) * 255.0f));
    px[3] = static_cast<unsigned char>(std::round(clamp01(color.a) * 255.0f));

    // Create texture from in-memory RGBA data
    return Texture::CreateFromMemory(rhi, px, 1, 1, 4, type, srgb);
}