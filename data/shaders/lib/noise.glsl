#load lib/constants.glsl

// ---- Hammersley Low-Discrepancy Series ----
float radicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), radicalInverse_VdC(i));
}  
// ----------------------------------------------------------------------------
float vanDerCorput(uint n, uint base)
{
    float invBase = 1.0 / float(base);
    float denom   = 1.0;
    float result  = 0.0;

    for(uint i = 0u; i < 32u; ++i)
    {
        if(n > 0u)
        {
            denom   = mod(float(n), 2.0);
            result += denom * invBase;
            invBase = invBase / 2.0;
            n       = uint(float(n) / 2.0);
        }
    }

    return result;
}
// ----------------------------------------------------------------------------
vec2 hammersleyNoBitOps(uint i, uint N)
{
    return vec2(float(i)/float(N), vanDerCorput(i, 2u));
}

// ---- Simplex Noise ----
#define  NORMALIZE_GRADIENTS
#undef  USE_CIRCLE
#define COLLAPSE_SORTNET

float permute(float x0,vec3 p) { 
    float x1 = mod(x0 * p.y, p.x);
    return floor(  mod( (x1 + p.z) *x0, p.x ));
}
vec2 permute(vec2 x0,vec3 p) { 
    vec2 x1 = mod(x0 * p.y, p.x);
    return floor(  mod( (x1 + p.z) *x0, p.x ));
}
vec3 permute(vec3 x0,vec3 p) { 
    vec3 x1 = mod(x0 * p.y, p.x);
    return floor(  mod( (x1 + p.z) *x0, p.x ));
}
vec4 permute(vec4 x0,vec3 p) { 
    vec4 x1 = mod(x0 * p.y, p.x);
    return floor(  mod( (x1 + p.z) *x0, p.x ));
}

// Example constant with a 289 element permutation
const vec4 pParam = vec4( 17.0*17.0, 34.0, 1.0, 7.0);

float taylorInvSqrt(float r)
{ 
    return ( 0.83666002653408 + 0.7*0.85373472095314 - 0.85373472095314 * r );
}

float simplexNoise2(vec2 v)
{
    const vec2 C = vec2(0.211324865405187134, // (3.0-sqrt(3.0))/6.;
                      0.366025403784438597); // 0.5*(sqrt(3.0)-1.);
    const vec3 D = vec3( 0., 0.5, 2.0) * PI;
// First corner
    vec2 i  = floor(v + dot(v, C.yy) );
    vec2 x0 = v -   i + dot(i, C.xx);

// Other corners
    vec2 i1  =  (x0.x > x0.y) ? vec2(1.,0.) : vec2(0.,1.) ;

   //  x0 = x0 - 0. + 0. * C
    vec2 x1 = x0 - i1 + 1. * C.xx ;
    vec2 x2 = x0 - 1. + 2. * C.xx ;


// Permutations
    i = mod(i, pParam.x);
    vec3 p = permute( permute( 
             i.y + vec3(0., i1.y, 1. ), pParam.xyz)
           + i.x + vec3(0., i1.x, 1. ), pParam.xyz);

#ifndef USE_CIRCLE
// ( N points uniformly over a line, mapped onto a diamond.)
    vec3 x = fract(p / pParam.w) ;
    vec3 h = 0.5 - abs(x) ;

    vec3 sx = vec3(lessThan(x,D.xxx)) *2. -1.;
    vec3 sh = vec3(lessThan(h,D.xxx));

    vec3 a0 = x + sx*sh;
    vec2 p0 = vec2(a0.x,h.x);
    vec2 p1 = vec2(a0.y,h.y);
    vec2 p2 = vec2(a0.z,h.z);

#ifdef NORMALISE_GRADIENTS
    p0 *= taylorInvSqrt(dot(p0,p0));
    p1 *= taylorInvSqrt(dot(p1,p1));
    p2 *= taylorInvSqrt(dot(p2,p2));
#endif

    vec3 g = 2.0 * vec3( dot(p0, x0), dot(p1, x1), dot(p2, x2) );
#else 
// N points around a unit circle.
    vec3 phi = D.z * mod(p,pParam.w) /pParam.w ;
    vec4 a0 = sin(phi.xxyy+D.xyxy);
    vec2 a1 = sin(phi.zz  +D.xy);
    vec3 g = vec3( dot(a0.xy, x0), dot(a0.zw, x1), dot(a1.xy, x2) );
#endif
// mix
    vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x1,x1), dot(x2,x2)), 0.);
    m = m*m ;
    return (1.66666* 70.*dot(m*m, g) + 1.0)/2.0;
}
// https://github.com/ashima/webgl-noise/blob/master/src/psrdnoise2D.glsl
// Modulo 289, optimizes to code without divisions
vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}
// Permutation polynomial (ring size 289 = 17*17)
vec3 permute(vec3 x) {
  return mod289(((x*34.0)+10.0)*x);
}
float mod289(float x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}
// Permutation polynomial (ring size 289 = 17*17)
float permute(float x) {
  return mod289(((x*34.0)+10.0)*x);
}
// Hashed 2-D gradients with an extra rotation.
// (The constant 0.0243902439 is 1/41)
vec2 rgrad2(vec2 p, float rot) {
#if 0
// Map from a line to a diamond such that a shift maps to a rotation.
  float u = permute(permute(p.x) + p.y) * 0.0243902439 + rot; // Rotate by shift
  u = 4.0 * fract(u) - 2.0;
  // (This vector could be normalized, exactly or approximately.)
  return vec2(abs(u)-1.0, abs(abs(u+1.0)-2.0)-1.0);
#else
// For more isotropic gradients, sin/cos can be used instead.
  float u = permute(permute(p.x) + p.y) * 0.0243902439 + rot; // Rotate by shift
  u = fract(u) * 6.28318530718; // 2*pi
  return vec2(cos(u), sin(u));
#endif
}
//
// 2-D non-tiling simplex noise with rotating gradients and analytical derivative.
// The first component of the 3-element return vector is the noise value,
// and the second and third components are the x and y partial derivatives.
//
vec3 srdnoise(vec2 pos) {
  // Offset y slightly to hide some rare artifacts
  pos.y += 0.001;
  // Skew to hexagonal grid
  vec2 uv = vec2(pos.x + pos.y*0.5, pos.y);
  
  vec2 i0 = floor(uv);
  vec2 f0 = fract(uv);
  // Traversal order
  vec2 i1 = (f0.x > f0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);

  // Unskewed grid points in (x,y) space
  vec2 p0 = vec2(i0.x - i0.y * 0.5, i0.y);
  vec2 p1 = vec2(p0.x + i1.x - i1.y * 0.5, p0.y + i1.y);
  vec2 p2 = vec2(p0.x + 0.5, p0.y + 1.0);

  // Integer grid point indices in (u,v) space
  i1 = i0 + i1;
  vec2 i2 = i0 + vec2(1.0, 1.0);

  // Vectors in unskewed (x,y) coordinates from
  // each of the simplex corners to the evaluation point
  vec2 d0 = pos - p0;
  vec2 d1 = pos - p1;
  vec2 d2 = pos - p2;

  vec3 x = vec3(p0.x, p1.x, p2.x);
  vec3 y = vec3(p0.y, p1.y, p2.y);
  vec3 iuw = x + 0.5 * y;
  vec3 ivw = y;
  
  // Avoid precision issues in permutation
  iuw = mod289(iuw);
  ivw = mod289(ivw);

  // Create gradients from indices
  vec2 g0 = rgrad2(vec2(iuw.x, ivw.x), 0.0);
  vec2 g1 = rgrad2(vec2(iuw.y, ivw.y), 0.0);
  vec2 g2 = rgrad2(vec2(iuw.z, ivw.z), 0.0);

  // Gradients dot vectors to corresponding corners
  // (The derivatives of this are simply the gradients)
  vec3 w = vec3(dot(g0, d0), dot(g1, d1), dot(g2, d2));
  
  // Radial weights from corners
  // 0.8 is the square of 2/sqrt(5), the distance from
  // a grid point to the nearest simplex boundary
  vec3 t = 0.8 - vec3(dot(d0, d0), dot(d1, d1), dot(d2, d2));

  // Partial derivatives for analytical gradient computation
  vec3 dtdx = -2.0 * vec3(d0.x, d1.x, d2.x);
  vec3 dtdy = -2.0 * vec3(d0.y, d1.y, d2.y);

  // Set influence of each surflet to zero outside radius sqrt(0.8)
  if (t.x < 0.0) {
    dtdx.x = 0.0;
    dtdy.x = 0.0;
	t.x = 0.0;
  }
  if (t.y < 0.0) {
    dtdx.y = 0.0;
    dtdy.y = 0.0;
	t.y = 0.0;
  }
  if (t.z < 0.0) {
    dtdx.z = 0.0;
    dtdy.z = 0.0;
	t.z = 0.0;
  }

  // Fourth power of t (and third power for derivative)
  vec3 t2 = t * t;
  vec3 t4 = t2 * t2;
  vec3 t3 = t2 * t;
  
  // Final noise value is:
  // sum of ((radial weights) times (gradient dot vector from corner))
  float n = dot(t4, w);
  
  // Final analytical derivative (gradient of a sum of scalar products)
  vec2 dt0 = vec2(dtdx.x, dtdy.x) * 4.0 * t3.x;
  vec2 dn0 = t4.x * g0 + dt0 * w.x;
  vec2 dt1 = vec2(dtdx.y, dtdy.y) * 4.0 * t3.y;
  vec2 dn1 = t4.y * g1 + dt1 * w.y;
  vec2 dt2 = vec2(dtdx.z, dtdy.z) * 4.0 * t3.z;
  vec2 dn2 = t4.z * g2 + dt2 * w.z;

  return (11.0*vec3(n, dn0 + dn1 + dn2) + 1.0) / 2.0;
}
float layeredSimplexNoise2(vec2 v, int octaves, float period, float persistance, float lacunarity){
   float res = 0;
   float p = 1.0;
   float avg = 0.0;
   for(int i = 0; i < octaves; i++){
      res += p*simplexNoise2(v*(80/period));
      p *= persistance;
      period /= lacunarity;
      avg += p;
   }
   return res / avg;
}
vec3 layeredNormalSimplexNoise2(vec2 v, int octaves, float period, float persistance, float lacunarity){
   vec3 res = vec3(0.0);
   float p = 1.0;
   float avg = 0.0;
   for(int i = 0; i < octaves; i++){
      res += p*srdnoise(v*(80/period));
      p *= persistance;
      period /= lacunarity;
      avg += p;
   }
   return res / avg;
}
// -------------------