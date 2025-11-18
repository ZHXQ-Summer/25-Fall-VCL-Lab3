#include "Labs/3-Rendering/tasks.h"

namespace VCX::Labs::Rendering {

    glm::vec4 GetTexture(Engine::Texture2D<Engine::Formats::RGBA8> const & texture, glm::vec2 const & uvCoord) {
        if (texture.GetSizeX() == 1 || texture.GetSizeY() == 1) return texture.At(0, 0);
        glm::vec2 uv      = glm::fract(uvCoord);
        uv.x              = uv.x * texture.GetSizeX() - .5f;
        uv.y              = uv.y * texture.GetSizeY() - .5f;
        std::size_t xmin  = std::size_t(glm::floor(uv.x) + texture.GetSizeX()) % texture.GetSizeX();
        std::size_t ymin  = std::size_t(glm::floor(uv.y) + texture.GetSizeY()) % texture.GetSizeY();
        std::size_t xmax  = (xmin + 1) % texture.GetSizeX();
        std::size_t ymax  = (ymin + 1) % texture.GetSizeY();
        float       xfrac = glm::fract(uv.x), yfrac = glm::fract(uv.y);
        return glm::mix(glm::mix(texture.At(xmin, ymin), texture.At(xmin, ymax), yfrac), glm::mix(texture.At(xmax, ymin), texture.At(xmax, ymax), yfrac), xfrac);
    }

    glm::vec4 GetAlbedo(Engine::Material const & material, glm::vec2 const & uvCoord) {
        glm::vec4 albedo       = GetTexture(material.Albedo, uvCoord);
        glm::vec3 diffuseColor = albedo;
        return glm::vec4(glm::pow(diffuseColor, glm::vec3(2.2)), albedo.w);
    }

    /******************* 1. Ray-triangle intersection *****************/
    bool IntersectTriangle(Intersection & output, Ray const & ray, glm::vec3 const & p1, glm::vec3 const & p2, glm::vec3 const & p3) {
        // your code here
    const float EPSILON = 1e-8f;
    glm::vec3 e1 = p1 - p2;
    glm::vec3 e2 = p1 - p3;
    glm::vec3 n = glm::cross(e1, e2);
    float dirDotN = glm::dot(ray.Direction, n);
    if (glm::abs(dirDotN) < EPSILON) {
        return false;
    }
    float t_n = glm::dot((p1 - ray.Origin), n) / dirDotN;
    if (t_n < EPSILON) { 
        return false;
    }
    glm::vec3 intersection = ray.Origin + t_n * ray.Direction;
    glm::vec3 v = intersection - p1;
    glm::vec3 edge1 = p2 - p1;  
    glm::vec3 edge2 = p3 - p1;  

    float dot00 = glm::dot(edge2, edge2);
    float dot01 = glm::dot(edge2, edge1);
    float dot02 = glm::dot(edge2, v);
    float dot11 = glm::dot(edge1, edge1);
    float dot12 = glm::dot(edge1, v);

    float denominator = dot00 * dot11 - dot01 * dot01;
    if (glm::abs(denominator) < EPSILON) {
        return false; 
    }

    float u = (dot00 * dot12 - dot01 * dot02) / denominator;
    float v_coord = (dot11 * dot02 - dot01 * dot12) / denominator;

    if (u >= -EPSILON && v_coord >= -EPSILON && (u + v_coord) <= 1.0f + EPSILON) {
        output.t = t_n;
        output.u = u;
        output.v = v_coord;
        return true;
    }

    return false;
    }

    glm::vec3 RayTrace(const RayIntersector & intersector, Ray ray, int maxDepth, bool enableShadow) {
        glm::vec3 color(0.0f);
        glm::vec3 weight(1.0f);

        for (int depth = 0; depth < maxDepth; depth++) {
            auto rayHit = intersector.IntersectRay(ray);
            if (! rayHit.IntersectState) return color;
            const glm::vec3 pos       = rayHit.IntersectPosition;
            const glm::vec3 n         = rayHit.IntersectNormal;
            const glm::vec3 kd        = rayHit.IntersectAlbedo;
            const glm::vec3 ks        = rayHit.IntersectMetaSpec;
            const float     alpha     = rayHit.IntersectAlbedo.w;
            const float     shininess = rayHit.IntersectMetaSpec.w * 256;

            glm::vec3 result(0.0f);
            /******************* 2. Whitted-style ray tracing *****************/
            // your code here
            result+=intersector.InternalScene->AmbientIntensity*kd;
            for (const Engine::Light & light : intersector.InternalScene->Lights) {
                glm::vec3 l;
                float     attenuation;
                bool visible=true;
                /******************* 3. Shadow ray *****************/
                if (light.Type == Engine::LightType::Point) {
                    l           = light.Position - pos;
                    attenuation = 1.0f / glm::dot(l, l);
                    if (enableShadow) {
                        // your code here
                        auto hit=intersector.IntersectRay(Ray(pos+1e-4f*n,glm::normalize(l)));
                        if(hit.IntersectState&&glm::length(hit.IntersectPosition-pos)<glm::length(l)){
                            visible=false;
                        }
                    }
                } else if (light.Type == Engine::LightType::Directional) {
                    l           = light.Direction;
                    attenuation = 1.0f;
                    if (enableShadow) {
                        auto hit=intersector.IntersectRay(Ray(pos+1e-4f*n,glm::normalize(l)));
                        if(hit.IntersectState){
                            visible=false;
                        }
                    }
                }
                if(!visible){
                    continue;
                }
                /******************* 2. Whitted-style ray tracing *****************/
                // your code here
                float nd=glm::dot(n,glm::normalize(l));
                nd=nd>0.0f?nd:0.0f;
                glm::vec3 v=glm::normalize(-ray.Direction);
                glm::vec3 r=glm::reflect(-glm::normalize(l),n);
                float ns =glm::dot(r, v);
                ns=ns>0.0f?ns:0.0f;
                result+=attenuation*(nd*kd+ks*glm::pow(ns,shininess))*light.Intensity;
            }

            if (alpha < 0.9) {
                // refraction
                // accumulate color
                glm::vec3 R = alpha * glm::vec3(1.0f);
                color += weight * R * result;
                weight *= glm::vec3(1.0f) - R;

                // generate new ray
                ray = Ray(pos, ray.Direction);
            } else {
                // reflection
                // accumulate color
                glm::vec3 R = ks * glm::vec3(0.5f);
                color += weight * (glm::vec3(1.0f) - R) * result;
                weight *= R;

                // generate new ray
                glm::vec3 out_dir = ray.Direction - glm::vec3(2.0f) * n * glm::dot(n, ray.Direction);
                ray               = Ray(pos, out_dir);
            }
        }

        return color;
    }
} // namespace VCX::Labs::Rendering