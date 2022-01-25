#include "FastSIMD/InlInclude.h"

#include "DomainWarp.h"
#include "CoherentHelpers.inl"

template<typename FS>
class FS_T<FastNoise::DomainWarp, FS> : public virtual FastNoise::DomainWarp, public FS_T<FastNoise::Generator, FS>
{
public:
    float GetWarpFrequency() const { return mWarpFrequency; }
    const HybridSource& GetWarpAmplitude() const { return mWarpAmplitude; }

    virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const = 0;
    virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const = 0;
    virtual void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const = 0;

    FASTNOISE_IMPL_GEN_T;

    template<typename... P>
    FS_INLINE float32v GenT( int32v seed, P... pos ) const
    {
        Warp( seed, this->GetSourceValue( mWarpAmplitude, seed, pos... ), (pos * float32v( mWarpFrequency ))..., pos... );

        return this->GetSourceValue( mSource, seed, pos...);
    }
};

template<typename FS>
class FS_T<FastNoise::DomainWarpGradient, FS> : public virtual FastNoise::DomainWarpGradient, public FS_T<FastNoise::DomainWarp, FS>
{
public:
    void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v& xOut, float32v& yOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );

    #define GRADIENT_COORD( _x, _y )\
        int32v hash##_x##_y = HashPrimesHB(seed, x##_x, y##_y );\
        float32v x##_x##_y = FS_Converti32_f32( hash##_x##_y & int32v( 0xffff ) );\
        float32v y##_x##_y = FS_Converti32_f32( (hash##_x##_y >> 16) & int32v( 0xffff ) );

        GRADIENT_COORD( 0, 0 );
        GRADIENT_COORD( 1, 0 );
        GRADIENT_COORD( 0, 1 );
        GRADIENT_COORD( 1, 1 );

    #undef GRADIENT_COORD

        float32v normalise = warpAmp * float32v( 1.0f / (0xffff / 2.0f) );

        xOut = FS_FMulAdd_f32( Lerp( Lerp( x00, x10, xs ), Lerp( x01, x11, xs ), ys ) - float32v( 0xffff / 2.0f ), normalise, xOut );
        yOut = FS_FMulAdd_f32( Lerp( Lerp( y00, y10, xs ), Lerp( y01, y11, xs ), ys ) - float32v( 0xffff / 2.0f ), normalise, yOut );
    }
            
    void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v& xOut, float32v& yOut, float32v& zOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( Primes::Z );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );

    #define GRADIENT_COORD( _x, _y, _z )\
        int32v hash##_x##_y##_z = HashPrimesHB( seed, x##_x, y##_y, z##_z );\
        float32v x##_x##_y##_z = FS_Converti32_f32( hash##_x##_y##_z & int32v( 0x3ff ) );\
        float32v y##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 10) & int32v( 0x3ff ) );\
        float32v z##_x##_y##_z = FS_Converti32_f32( (hash##_x##_y##_z >> 20) & int32v( 0x3ff ) );

        GRADIENT_COORD( 0, 0, 0 );
        GRADIENT_COORD( 1, 0, 0 );
        GRADIENT_COORD( 0, 1, 0 );
        GRADIENT_COORD( 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 1 );
        GRADIENT_COORD( 0, 1, 1 );
        GRADIENT_COORD( 1, 1, 1 );

    #undef GRADIENT_COORD

        float32v x0z = Lerp( Lerp( x000, x100, xs ), Lerp( x010, x110, xs ), ys );
        float32v y0z = Lerp( Lerp( y000, y100, xs ), Lerp( y010, y110, xs ), ys );
        float32v z0z = Lerp( Lerp( z000, z100, xs ), Lerp( z010, z110, xs ), ys );
                   
        float32v x1z = Lerp( Lerp( x001, x101, xs ), Lerp( x011, x111, xs ), ys );
        float32v y1z = Lerp( Lerp( y001, y101, xs ), Lerp( y011, y111, xs ), ys );
        float32v z1z = Lerp( Lerp( z001, z101, xs ), Lerp( z011, z111, xs ), ys );

        float32v normalise = warpAmp * float32v( 1.0f / (0x3ff / 2.0f) );

        xOut = FS_FMulAdd_f32( Lerp( x0z, x1z, zs ) - float32v( 0x3ff / 2.0f ), normalise, xOut );
        yOut = FS_FMulAdd_f32( Lerp( y0z, y1z, zs ) - float32v( 0x3ff / 2.0f ), normalise, yOut );
        zOut = FS_FMulAdd_f32( Lerp( z0z, z1z, zs ) - float32v( 0x3ff / 2.0f ), normalise, zOut );
    }
            
    void FS_VECTORCALL Warp( int32v seed, float32v warpAmp, float32v x, float32v y, float32v z, float32v w, float32v& xOut, float32v& yOut, float32v& zOut, float32v& wOut ) const final
    {
        float32v xs = FS_Floor_f32( x );
        float32v ys = FS_Floor_f32( y );
        float32v zs = FS_Floor_f32( z );
        float32v ws = FS_Floor_f32( w );

        int32v x0 = FS_Convertf32_i32( xs ) * int32v( Primes::X );
        int32v y0 = FS_Convertf32_i32( ys ) * int32v( Primes::Y );
        int32v z0 = FS_Convertf32_i32( zs ) * int32v( Primes::Z );
        int32v w0 = FS_Convertf32_i32( ws ) * int32v( Primes::W );
        int32v x1 = x0 + int32v( Primes::X );
        int32v y1 = y0 + int32v( Primes::Y );
        int32v z1 = z0 + int32v( Primes::Z );
        int32v w1 = w0 + int32v( Primes::W );

        xs = InterpHermite( x - xs );
        ys = InterpHermite( y - ys );
        zs = InterpHermite( z - zs );
        ws = InterpHermite( w - ws );

    #define GRADIENT_COORD( _x, _y, _z, _w )\
        int32v hash##_x##_y##_z##_w = HashPrimesHB( seed, x##_x, y##_y, z##_z, w##_w );\
        float32v x##_x##_y##_z##_w = FS_Converti32_f32( hash##_x##_y##_z##_w & int32v( 0xff ) );\
        float32v y##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 8) & int32v( 0xff ) );\
        float32v z##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 16) & int32v( 0xff ) );\
        float32v w##_x##_y##_z##_w = FS_Converti32_f32( (hash##_x##_y##_z##_w >> 24) & int32v( 0xff ) );

        GRADIENT_COORD( 0, 0, 0, 0 );
        GRADIENT_COORD( 1, 0, 0, 0 );
        GRADIENT_COORD( 0, 1, 0, 0 );
        GRADIENT_COORD( 1, 1, 0, 0 );
        GRADIENT_COORD( 0, 0, 1, 0 );
        GRADIENT_COORD( 1, 0, 1, 0 );
        GRADIENT_COORD( 0, 1, 1, 0 );
        GRADIENT_COORD( 1, 1, 1, 0 );
        GRADIENT_COORD( 0, 0, 0, 1 );
        GRADIENT_COORD( 1, 0, 0, 1 );
        GRADIENT_COORD( 0, 1, 0, 1 );
        GRADIENT_COORD( 1, 1, 0, 1 );
        GRADIENT_COORD( 0, 0, 1, 1 );
        GRADIENT_COORD( 1, 0, 1, 1 );
        GRADIENT_COORD( 0, 1, 1, 1 );
        GRADIENT_COORD( 1, 1, 1, 1 );

    #undef GRADIENT_COORD

        float32v x0w = Lerp( Lerp( Lerp( x0000, x1000, xs ), Lerp( x0100, x1100, xs ), ys ), Lerp( Lerp( x0010, x1010, xs ), Lerp( x0110, x1110, xs ), ys ), zs );
        float32v y0w = Lerp( Lerp( Lerp( y0000, y1000, xs ), Lerp( y0100, y1100, xs ), ys ), Lerp( Lerp( y0010, y1010, xs ), Lerp( y0110, y1110, xs ), ys ), zs );
        float32v z0w = Lerp( Lerp( Lerp( z0000, z1000, xs ), Lerp( z0100, z1100, xs ), ys ), Lerp( Lerp( z0010, z1010, xs ), Lerp( z0110, z1110, xs ), ys ), zs );
        float32v w0w = Lerp( Lerp( Lerp( w0000, w1000, xs ), Lerp( w0100, w1100, xs ), ys ), Lerp( Lerp( w0010, w1010, xs ), Lerp( w0110, w1110, xs ), ys ), zs );

        float32v x1w = Lerp( Lerp( Lerp( x0001, x1001, xs ), Lerp( x0101, x1101, xs ), ys ), Lerp( Lerp( x0011, x1011, xs ), Lerp( x0111, x1111, xs ), ys ), zs );
        float32v y1w = Lerp( Lerp( Lerp( y0001, y1001, xs ), Lerp( y0101, y1101, xs ), ys ), Lerp( Lerp( y0011, y1011, xs ), Lerp( y0111, y1111, xs ), ys ), zs );
        float32v z1w = Lerp( Lerp( Lerp( z0001, z1001, xs ), Lerp( z0101, z1101, xs ), ys ), Lerp( Lerp( z0011, z1011, xs ), Lerp( z0111, z1111, xs ), ys ), zs );
        float32v w1w = Lerp( Lerp( Lerp( w0001, w1001, xs ), Lerp( w0101, w1101, xs ), ys ), Lerp( Lerp( w0011, w1011, xs ), Lerp( w0111, w1111, xs ), ys ), zs );                        

        float32v normalise = warpAmp * float32v( 1.0f / (0xff / 2.0f) );

        xOut = FS_FMulAdd_f32( Lerp( x0w, x1w, ws ) - float32v( 0xff / 2.0f ), normalise, xOut );
        yOut = FS_FMulAdd_f32( Lerp( y0w, y1w, ws ) - float32v( 0xff / 2.0f ), normalise, yOut );
        zOut = FS_FMulAdd_f32( Lerp( z0w, z1w, ws ) - float32v( 0xff / 2.0f ), normalise, zOut );
        wOut = FS_FMulAdd_f32( Lerp( w0w, w1w, ws ) - float32v( 0xff / 2.0f ), normalise, wOut );
    }
};

