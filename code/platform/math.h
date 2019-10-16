#if !defined(MATH_H)

//TODO(bjorn): Second pass on all of the math stuff with SIMD and also some unit testing.

#include "types_and_defines.h"

inline s32 FloorF32ToS32(f32 Number);
inline f32 Sin(f32 Value);
inline f32 Cos(f32 Value);

#include "intrinsics.h"

struct v2s;
struct v3s;
struct v2u;
struct v3u;
struct v2;
struct v3;
struct v4;
struct m22;
struct m33;
struct m44;

struct v2s
{
	union
	{
		struct
		{
			s32 X;
			s32 Y;
		};
		s32 E_[2];
	};
  v2s&
    operator+=(v2s rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      return *this;
    }
  v2s&
    operator-=(v2s rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      return *this;
    }
  v2s&
    operator/=(s32 rhs)
    {
      this->X = this->X / rhs;
      this->Y = this->Y / rhs;
      return *this;
    }
  v2s&
    operator*=(s32 rhs)
    {
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      return *this;
    }
	operator v3s();
	operator v2();
};

inline v2s
operator+(v2s lhs, v2s rhs)
{
  lhs += rhs;
  return lhs;
}
inline v2s
operator-(v2s lhs, v2s rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v2s
operator/(v2s lhs, s32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v2s
operator*(v2s lhs, s32 rhs)
{
  lhs *= rhs;
  return lhs;
}
inline v2s
operator-(v2s rhs)
{
	rhs.X = -rhs.X;
	rhs.Y = -rhs.Y; 
	return rhs;
}
struct v3s
{
	union
	{
		s32 E_[3];
		struct
		{
			s32 X;
			s32 Y;
			s32 Z;
		};
		struct
		{
			s32 R;
			s32 G;
			s32 B;
		};
		v2s XY;
		struct
		{
			s32 ___;
			v2s YZ;
		};
	};
  v3s&
    operator+=(v3s rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      this->Z = this->Z + rhs.Z;
      return *this;
    }
  v3s&
    operator-=(v3s rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      this->Z = this->Z - rhs.Z;
      return *this;
    }
  v3s&
    operator/=(s32 rhs)
    {
      this->X = this->X / rhs;
      this->Y = this->Y / rhs;
      this->Z = this->Z / rhs;
      return *this;
    }
  v3s&
    operator*=(s32 rhs)
    {
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      this->Z = this->Z * rhs;
      return *this;
    }
	operator v3();
};

inline v3s
operator+(v3s lhs, v3s rhs)
{
  lhs += rhs;
  return lhs;
}
inline v3s
operator-(v3s lhs, v3s rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v3s
operator/(v3s lhs, s32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v3s
operator*(v3s lhs, s32 rhs)
{
  lhs *= rhs;
  return lhs;
}
inline v3s
operator-(v3s rhs)
{
	rhs.X = -rhs.X;
	rhs.Y = -rhs.Y; 
	rhs.Z = -rhs.Z; 
	return rhs;
}
inline bool
operator==(v3s lhs, v3s rhs)
{
  return (lhs.X == rhs.X &&
					lhs.Y == rhs.Y &&
					lhs.Z == rhs.Z );
}

struct v2u
{
	union
	{
		struct
		{
			u32 X;
			u32 Y;
		};
		u32 E_[2];
	};
  v2u&
    operator+=(v2u rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      return *this;
    }
  v2u&
    operator-=(v2u rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      return *this;
    }
  v2u&
    operator/=(u32 rhs)
    {
      this->X = this->X / rhs;
      this->Y = this->Y / rhs;
      return *this;
    }
  v2u&
    operator*=(u32 rhs)
    {
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      return *this;
    }
	operator v2();
	operator v2s();
};

inline v2u
operator+(v2u lhs, v2u rhs)
{
  lhs += rhs;
  return lhs;
}
inline v2u
operator-(v2u lhs, v2u rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v2u
operator/(v2u lhs, u32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v2u
operator*(v2u lhs, u32 rhs)
{
  lhs *= rhs;
  return lhs;
}

struct v3u
{
	union
	{
		u32 E_[3];
		struct
		{
			u32 X;
			u32 Y;
			u32 Z;
		};
		struct
		{
			u32 R;
			u32 G;
			u32 B;
		};
		v2u XY;
		struct
		{
			u32 ___;
			v2u YZ;
		};
	};
  v3u&
    operator+=(v3u rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      this->Z = this->Z + rhs.Z;
      return *this;
    }
  v3u&
    operator+=(v3s rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      this->Z = this->Z + rhs.Z;
      return *this;
    }
  v3u&
    operator-=(v3u rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      this->Z = this->Z - rhs.Z;
      return *this;
    }
  v3u&
    operator/=(u32 rhs)
    {
      this->X = this->X / rhs;
      this->Y = this->Y / rhs;
      this->Z = this->Z / rhs;
      return *this;
    }
  v3u&
    operator*=(u32 rhs)
    {
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      this->Z = this->Z * rhs;
      return *this;
    }
	operator v3();
	operator v3s();
};

inline v3u
operator+(v3u lhs, v3u rhs)
{
  lhs += rhs;
  return lhs;
}
inline v3u
operator-(v3u lhs, v3u rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v3u
operator/(v3u lhs, u32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v3u
operator*(v3u lhs, u32 rhs)
{
  lhs *= rhs;
  return lhs;
}

struct v2
{
	union
	{
		struct
		{
			f32 X;
			f32 Y;
		};
		f32 E_[2];
	};
  v2&
    operator+=(v2 rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      return *this;
    }
  v2&
    operator-=(v2 rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      return *this;
    }
  v2&
    operator/=(f32 rhs)
    {
      this->X = this->X / rhs;
      this->Y = this->Y / rhs;
      return *this;
    }
  v2&
    operator*=(f32 rhs)
    {
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      return *this;
    }
	v2& operator *=(m22 lhs);
	operator v3();
};

inline v2
operator+(v2 lhs, v2 rhs)
{
  lhs += rhs;
  return lhs;
}
inline v2
operator-(v2 lhs, v2 rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v2
operator-(v2 lhs, v2s rhs)
{
  lhs -= (v2)rhs;
  return lhs;
}
inline v2
operator/(v2 lhs, f32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v2
operator/(v2u lhs, f32 rhs)
{
  return {lhs.X / rhs, lhs.Y / rhs};
}
inline v2
operator*(v2 lhs, f32 rhs)
{
  lhs *= rhs;
  return lhs;
}
inline v2
operator*(f32 lhs, v2 rhs)
{
  return {rhs.X * lhs, rhs.Y * lhs};
}
inline v2
operator*(v2 lhs, s32 rhs)
{
  lhs *= (f32)rhs;
  return lhs;
}
inline v2
operator*(v2s lhs, f32 rhs)
{
  return {lhs.X * rhs, lhs.Y * rhs};
}
inline v2
operator-(v2 rhs)
{
	rhs.X = -rhs.X;
	rhs.Y = -rhs.Y;
  return rhs;
}

struct v3
{
	union
	{
		f32 E_[3];
		struct
		{
			f32 X;
			f32 Y;
			f32 Z;
		};
		struct
		{
			f32 R;
			f32 G;
			f32 B;
		};
		v2 XY;
		struct
		{
			f32 ___;
			v2 YZ;
		};
	};
	v3&
		operator+=(v3 rhs)
		{
			this->X = this->X + rhs.X;
			this->Y = this->Y + rhs.Y;
			this->Z = this->Z + rhs.Z;
			return *this;
		}
	v3&
		operator-=(v3 rhs)
		{
			this->X = this->X - rhs.X;
			this->Y = this->Y - rhs.Y;
			this->Z = this->Z - rhs.Z;
			return *this;
		}
	v3&
		operator/=(f32 rhs)
		{
			f32 i_rhs = 1.0f / rhs;
			this->X = this->X * i_rhs;
			this->Y = this->Y * i_rhs;
			this->Z = this->Z * i_rhs;
			return *this;
		}
	v3&
		operator*=(f32 rhs)
		{
			this->X = this->X * rhs;
			this->Y = this->Y * rhs;
			this->Z = this->Z * rhs;
			return *this;
		}
};

inline bool
operator==(v3 lhs, v3 rhs)
{
  return (lhs.X == rhs.X &&
					lhs.Y == rhs.Y &&
					lhs.Z == rhs.Z );
}
inline bool
operator!=(v3 lhs, v3 rhs)
{
  return (lhs.X != rhs.X &&
					lhs.Y != rhs.Y &&
					lhs.Z != rhs.Z );
}
inline v3
operator+(v3 lhs, v3 rhs)
{
  lhs += rhs;
  return lhs;
}
inline v3
operator-(v3 lhs, v3 rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v3
operator/(v3 lhs, f32 rhs)
{
  lhs /= rhs;
  return lhs;
}
inline v3
operator*(v3 lhs, f32 rhs)
{
  lhs *= rhs;
  return lhs;
}
inline v3
operator*(f32 lhs, v3 rhs)
{
  rhs *= lhs;
  return rhs;
}
inline v3
operator*(v3s lhs, f32 rhs)
{
	v3 Result = (v3)lhs;
  Result *= rhs;
  return Result;
}
inline v3
operator-(v3 rhs)
{
	rhs.X = -rhs.X;
	rhs.Y = -rhs.Y; 
	rhs.Z = -rhs.Z; 
	return rhs;
}

struct m22
{
	union
	{
		struct
		{
			f32 A; f32 B;
			f32 C; f32 D;
		};
		f32 E_[4];
	};

	m22&
		operator*=(m22 rhs)
		{
			f32 _A = this->A * rhs.A + this->B * rhs.C; 
			f32 _B = this->A * rhs.B + this->B * rhs.D;
			f32 _C = this->C * rhs.A + this->D * rhs.C; 
			f32 _D = this->C * rhs.B + this->D * rhs.D;
			this->A = _A;
			this->B = _B;
			this->C = _C;
			this->D = _D;
			return *this;
		}
};

	inline m22
M22ByRow(v2 A, v2 B)
{
	return {A.X, A.Y, 
					B.X, B.Y};
}

	inline m22
M22ByCol(v2 A, v2 B)
{
	return {A.X, B.X, 
					A.Y, B.Y};
}

inline v2
operator*(m22 lhs, v2 rhs)
{
	v2 Result = {};
	Result.X = lhs.A * rhs.X + lhs.B * rhs.Y;
	Result.Y = lhs.C * rhs.X + lhs.D * rhs.Y;
	return Result;
}
	inline m22
operator*(m22 lhs, m22 rhs)
{
	lhs *= rhs;
	return lhs;
}

struct m33
{
	union
	{
		struct
		{
			f32 A; f32 B; f32 C; 
			f32 D; f32 E; f32 F;
			f32 G; f32 H; f32 I;
		};
		f32 E_[9];
	};

	m33&
		operator*=(m33 rhs)
		{
			f32 _A = this->A * rhs.A + this->B * rhs.D + this->C * rhs.G; 
			f32 _B = this->A * rhs.B + this->B * rhs.E + this->C * rhs.H; 
			f32 _C = this->A * rhs.C + this->B * rhs.F + this->C * rhs.I; 

			f32 _D = this->D * rhs.A + this->E * rhs.D + this->F * rhs.G; 
			f32 _E = this->D * rhs.B + this->E * rhs.E + this->F * rhs.H; 
			f32 _F = this->D * rhs.C + this->E * rhs.F + this->F * rhs.I; 

			f32 _G = this->G * rhs.A + this->H * rhs.D + this->I * rhs.G; 
			f32 _H = this->G * rhs.B + this->H * rhs.E + this->I * rhs.H; 
			f32 _I = this->G * rhs.C + this->H * rhs.F + this->I * rhs.I; 

			this->A = _A; this->B = _B; this->C = _C;
			this->D = _D; this->E = _E; this->F = _F;
			this->G = _G; this->H = _H; this->I = _I;

			return *this;
		}
	m33&
		operator*=(f32 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] *= rhs;
			}

			return *this;
		}
	m33&
		operator+=(m33 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] += rhs.E_[ScalarIndex];
			}

			return *this;
		}
	m33&
		operator-=(m33 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] -= rhs.E_[ScalarIndex];
			}

			return *this;
		}
};

inline m33
M33Identity()
{
	return {1, 0, 0,
					0, 1, 0,
					0, 0, 1};
}

	inline m33
M33ByCol(v3 A, v3 B, v3 C)
{
	return {A.X, B.X, C.X,
					A.Y, B.Y, C.Y,
					A.Z, B.Z, C.Z};
}

	inline m33
M33TransformByCol(v2 A, v2 B, v2 C)
{
	return {A.X,  B.X,  C.X,
					A.Y,  B.Y,  C.Y,
					0.0f, 0.0f, 1.0f};
}

	inline m33
M33ByRow(v3 A, v3 B, v3 C)
{
	return {A.X, A.Y, A.Z,
					B.X, B.Y, B.Z,
					C.X, C.Y, C.Z};
}

inline v3
operator*(m33 lhs, v3 rhs)
{
	v3 Result = {};
	Result.X = lhs.A * rhs.X + lhs.B * rhs.Y + lhs.C * rhs.Z;
	Result.Y = lhs.D * rhs.X + lhs.E * rhs.Y + lhs.F * rhs.Z;
	Result.Z = lhs.G * rhs.X + lhs.H * rhs.Y + lhs.I * rhs.Z;
	return Result;
}
inline v2
operator*(m33 lhs, v2 rhs)
{
	v2 Result = {};
	Result.X = lhs.A * rhs.X + lhs.B * rhs.Y + lhs.C;
	Result.Y = lhs.D * rhs.X + lhs.E * rhs.Y + lhs.F;
	return Result;
}
	inline m33
operator*(m33 lhs, m33 rhs)
{
	lhs *= rhs;
	return lhs;
}
	inline m33
operator+(m33 lhs, m33 rhs)
{
	lhs += rhs;
	return lhs;
}
	inline m33
operator-(m33 lhs, m33 rhs)
{
	lhs -= rhs;
	return lhs;
}
	inline m33
operator*(m33 lhs, f32 rhs)
{
	lhs *= rhs;
	return lhs;
}

struct v4
{
	union
	{
		f32 E_[4];
		struct
		{
			f32 c;
			f32 i;
			f32 j;
			f32 k;
		};
		struct
		{
			f32 X;
			f32 Y;
			f32 Z;
			f32 W;
		};
		struct
		{
			f32 R;
			f32 G;
			f32 B;
			f32 A;
		};
		union
		{
			v3 XYZ;
			v3 RGB;
		};
		struct
		{
			f32 ___0;
			v3 YZW;
		};
		v2 XY;
		struct
		{
			f32 ___1;
			v2 YZ;
			f32 ___2;
		};
		struct
		{
			f32 ___3;
			f32 ___4;
			v2 ZW;
		};
	};
  v4&
    operator+=(v4 rhs)
    {
      this->X = this->X + rhs.X;
      this->Y = this->Y + rhs.Y;
      this->Z = this->Z + rhs.Z;
      this->W = this->W + rhs.W;
      return *this;
    }
  v4&
    operator-=(v4 rhs)
    {
      this->X = this->X - rhs.X;
      this->Y = this->Y - rhs.Y;
      this->Z = this->Z - rhs.Z;
      this->W = this->W - rhs.W;
      return *this;
    }
	v4&
		operator*=(f32 rhs)
		{
      this->X = this->X * rhs;
      this->Y = this->Y * rhs;
      this->Z = this->Z * rhs;
      this->W = this->W * rhs;
      return *this;
		}
  v4&
    operator*=(v4 rhs)
    {
			f32 _c = this->c*rhs.c - this->i*rhs.i - this->j*rhs.j - this->k*rhs.k;
			f32 _i = this->c*rhs.i + this->i*rhs.c + this->j*rhs.k - this->k*rhs.j;
			f32 _j = this->c*rhs.j - this->i*rhs.k + this->j*rhs.c + this->k*rhs.i;
			f32 _k = this->c*rhs.k + this->i*rhs.j - this->j*rhs.i + this->k*rhs.c;
			this->c = _c;
			this->i = _i;
			this->j = _j;
			this->k = _k;
			return *this;
		}
};
typedef v4 q;

inline bool
operator==(v4 lhs, v4 rhs)
{
  return (lhs.X == rhs.X &&
					lhs.Y == rhs.Y &&
					lhs.Z == rhs.Z &&
					lhs.W == rhs.W );
}
inline v4
operator+(v4 lhs, v4 rhs)
{
  lhs += rhs;
  return lhs;
}
inline v4
operator-(v4 lhs, v4 rhs)
{
  lhs -= rhs;
  return lhs;
}
inline v4
operator*(v4 lhs, f32 rhs)
{
  lhs *= rhs;
  return lhs;
}
inline v4
operator*(f32 lhs, v4 rhs)
{
  rhs *= lhs;
  return rhs;
}
inline v4
operator*(v4 lhs, v4 rhs)
{
  lhs *= rhs;
  return lhs;
}
	inline v4
V4(v3 Vec, f32 Scalar)
{
	return v4{Vec.X, Vec.Y, Vec.Z, Scalar};
}

inline q
QuaternionIdentity()
{
	return q{1, 0, 0, 0};
}

struct m44
{
	f32 E_[16];

	m44&
		operator*=(m44 rhs)
		{
			f32 _E[4];
			for(u32 i = 0;
					i < 16;
					i++)
			{
				u32 x = i % 4;
				u32 y = i / 4;
				_E[x] = (this->E_[4*y+0]*rhs.E_[4*0+x] + 
								 this->E_[4*y+1]*rhs.E_[4*1+x] + 
								 this->E_[4*y+2]*rhs.E_[4*2+x] + 
								 this->E_[4*y+3]*rhs.E_[4*3+x]);
				if(x == 3)
				{
					this->E_[i-3] = _E[x-3];
					this->E_[i-2] = _E[x-2];
					this->E_[i-1] = _E[x-1];
					this->E_[i-0] = _E[x-0];
				}
			}

			return *this;
		}
	m44&
		operator*=(f32 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] *= rhs;
			}

			return *this;
		}
	m44&
		operator+=(m44 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] += rhs.E_[ScalarIndex];
			}

			return *this;
		}
	m44&
		operator-=(m44 rhs)
		{
			for(u32 ScalarIndex = 0;
					ScalarIndex < ArrayCount(this->E_);
					ScalarIndex++)
			{
				this->E_[ScalarIndex] -= rhs.E_[ScalarIndex];
			}

			return *this;
		}
};

inline m44
M44Identity()
{
	return {1, 0, 0, 0,
					0, 1, 0, 0,
					0, 0, 1, 0,
					0, 0, 0, 1};
}

inline v4
operator*(m44 lhs, v4 rhs)
{
	v4 Result = {};
	Result.X = lhs.E_[ 0] * rhs.X + lhs.E_[ 1] * rhs.Y + lhs.E_[ 2] * rhs.Z + lhs.E_[ 3] * rhs.W;
	Result.Y = lhs.E_[ 4] * rhs.X + lhs.E_[ 5] * rhs.Y + lhs.E_[ 6] * rhs.Z + lhs.E_[ 7] * rhs.W;
	Result.Z = lhs.E_[ 8] * rhs.X + lhs.E_[ 9] * rhs.Y + lhs.E_[10] * rhs.Z + lhs.E_[11] * rhs.W;
	Result.W = lhs.E_[12] * rhs.X + lhs.E_[13] * rhs.Y + lhs.E_[14] * rhs.Z + lhs.E_[15] * rhs.W;

	return Result;
}
inline v3
operator*(m44 lhs, v3 rhs)
{
	return (lhs * V4(rhs, 1.0f)).XYZ;
}
	inline m44
operator*(m44 lhs, m44 rhs)
{
	lhs *= rhs;
	return lhs;
}
	inline m44
operator+(m44 lhs, m44 rhs)
{
	lhs += rhs;
	return lhs;
}
	inline m44
operator-(m44 lhs, m44 rhs)
{
	lhs -= rhs;
	return lhs;
}
	inline m44
operator*(m44 lhs, f32 rhs)
{
	lhs *= rhs;
	return lhs;
}

	inline v2& v2::operator
*=(m22 lhs)
{
	f32 _X = lhs.A * this->X + lhs.B * this->Y;
	f32 _Y = lhs.C * this->X + lhs.D * this->Y;
	this->X = _X;
	this->Y = _Y;
	return *this;
}
	inline v2::operator 
v3()
{
	return {v2::X, v2::Y, 0};
}
	inline v3u::operator 
v3()
{
	return {(f32)v3u::X, (f32)v3u::Y, (f32)v3u::Z};
}
	inline v3s::operator 
v3()
{
	return {(f32)v3s::X, (f32)v3s::Y, (f32)v3s::Z};
}
	inline v3u::operator 
v3s()
{
	return {(s32)v3u::X, (s32)v3u::Y, (s32)v3u::Z};
}
	inline v2s::operator 
v3s()
{
	return {v2s::X, v2s::Y, 0};
}
	inline v2s::operator 
v2()
{
	return {(f32)v2s::X, (f32)v2s::Y};
}
inline v2u::operator 
v2()
{
	return {(f32)v2u::X, (f32)v2u::Y};
}
inline v2u::operator 
v2s()
{
	return {(s32)v2u::X, (s32)v2u::Y};
}

struct increasing_from_origo
{
	v2 Lower;
	v2 Upper;
};

inline increasing_from_origo
MakeVectorsIncreasingFromOrigo(v2 MaybeLower, v2 MaybeUpper)
{
	increasing_from_origo Result;

	b32 XNotFlipped = MaybeLower.X < MaybeUpper.X;
	b32 YNotFlipped = MaybeLower.Y < MaybeUpper.Y;

	Result.Lower.X = XNotFlipped ? MaybeLower.X : MaybeUpper.X;
	Result.Lower.Y = YNotFlipped ? MaybeLower.Y : MaybeUpper.Y;
	Result.Upper.X = XNotFlipped ? MaybeUpper.X : MaybeLower.X;
	Result.Upper.Y = YNotFlipped ? MaybeUpper.Y : MaybeLower.Y;

	return Result;
}

// TODO(bjorn): Not safe. Lower and upper might be flipped?
inline b32 
InBounds(v2s Point, v2s Lower, v2s Upper)
{
	return ((Lower.X <= Point.X && Point.X <= Upper.X) &&
					(Lower.Y <= Point.Y && Point.Y <= Upper.Y));
}
inline b32
InBounds(v2 Point, v2 Lower, v2 Upper)
{
	increasing_from_origo Strict = MakeVectorsIncreasingFromOrigo(Lower, Upper);

	b32 XInBounds = (Strict.Lower.X <= Point.X && Point.X <= Strict.Upper.X);
	b32 YInBounds = (Strict.Lower.Y <= Point.Y && Point.Y <= Strict.Upper.Y);

	return (XInBounds && YInBounds);
}

inline v2
Hadamard(v2 A, v2 B)
{
	v2 Result;

	Result.X = A.X * B.X;
	Result.Y = A.Y * B.Y;

	return Result;
}
inline v3
Hadamard(v3 A, v3 B)
{
	v3 Result;

	Result.X = A.X * B.X;
	Result.Y = A.Y * B.Y;
	Result.Z = A.Z * B.Z;

	return Result;
}
inline v3
HadamardDiv(v3 A, v3 B)
{
	v3 Result;

	Result.X = A.X / B.X;
	Result.Y = A.Y / B.Y;
	Result.Z = A.Z / B.Z;

	return Result;
}
inline f32
Dot(v2 A, v2 B)
{
	f32 Result = A.X*B.X + A.Y*B.Y;
	
	return Result;
}
inline f32
Dot(v3 A, v3 B)
{
	f32 Result = A.X*B.X + A.Y*B.Y + A.Z*B.Z;
	
	return Result;
}
inline v3
Cross(v3 A, v3 B)
{
	v3 Result = {};

	Result.X = A.Y*B.Z - A.Z*B.Y;
	Result.Y = A.Z*B.X - A.X*B.Z;
	Result.Z = A.X*B.Y - A.Y*B.X;
	
	return Result;
}

//TODO(bjorn): Where does this belong.
struct dimensions
{
	v2 Min;
	v2 Max;
	f32 Width() { return this->Max.X - this->Min.X; } 
	f32 Height() { return this->Max.Y - this->Min.Y; }
};

inline u8 
EndianSwap(u8 Endian)
{
	return Endian;
}
inline s8 
EndianSwap(s8 Endian)
{
	return Endian;
}
inline u16 
EndianSwap(u16 Endian)
{
	return (((Endian & 0x00FF) << 8) | 
					((Endian & 0xFF00) >> 8));
}
inline s16 
EndianSwap(s16 Endian)
{
	return (s16)EndianSwap((u16)Endian);
}
inline u32 
EndianSwap(u32 Endian)
{
	return (((Endian & 0x000000FF) << 24) | 
					((Endian & 0x0000FF00) << 8) |
					((Endian & 0x00FF0000) >> 8) |
					((Endian & 0xFF000000) >> 24));
}
inline s32 
EndianSwap(s32 Endian)
{
	return (s32)EndianSwap((u32)Endian);
}
inline u64
EndianSwap(u64 Endian)
{
	return (((Endian & 0x00000000000000FF) << 56) | 
					((Endian & 0x000000000000FF00) << 40) | 
					((Endian & 0x0000000000FF0000) << 24) | 
					((Endian & 0x00000000FF000000) << 8) |
					((Endian & 0x000000FF00000000) >> 8) |
					((Endian & 0x0000FF0000000000) >> 24) |
					((Endian & 0x00FF000000000000) >> 40) |
					((Endian & 0xFF00000000000000) >> 56));
}
inline s64
EndianSwap(s64 Endian)
{
	return (s64)EndianSwap((u64)Endian);
}

  inline s32
RoundF32ToS32(f32 Number)
{
  return Number >= 0.0f ? (s32)(Number + 0.5f) : (s32)(Number - 0.5f);
}
  inline v2s
RoundV2ToV2S(v2 A)
{
  return {RoundF32ToS32(A.X), RoundF32ToS32(A.Y)};
}
  inline v3s
RoundV3ToV3S(v3 A)
{
  return {RoundF32ToS32(A.X), RoundF32ToS32(A.Y), RoundF32ToS32(A.Z)};
}

  inline s32
FloorF32ToS32(f32 Number)
{
  if(Number < 0)
  {
    Number -= 1.0f;
  }
  return (s32)(Number);
}

  inline s32
RoofF32ToS32(f32 Number)
{
	Number += 1.0f;
  if(Number < 0)
  {
    Number -= 1.0f;
  }
  return (s32)(Number);
}

  inline s32
TruncateF32ToS32(f32 Number)
{
  return (s32)(Number);
}

internal_function u32
SafeTruncateU64(u64 Value)
{
  // TODO(bjorn): Defines for maximum values u32MAX etc.
  Assert(Value <= 0xFFFFFFFF);
  return (u32)Value;
}

inline m22
CWM22(f32 Angle)
{
	return { Cos(Angle), Sin(Angle), 
		      -Sin(Angle), Cos(Angle)};
}
inline m22
CW90M22()
{
	return { 0, 1, 
		      -1, 0};
}
inline m22
CCWM22(f32 Angle)
{
	return { Cos(Angle),-Sin(Angle), 
		       Sin(Angle), Cos(Angle)};
}
inline m22
CCW90M22()
{
	return { 0,-1, 
		       1, 0};
}

//NOTE(bjorn): The quake fast inverse square.
	inline f32
InverseSquareRoot(f32 Number)
{
	s32 i;
	f32 x2, y;
	f32 ThreeHalfs = 1.5f;

	x2 = Number * 0.5f;
	y  = Number;

	i  = *(s32*)&y;                       // evil floating point bit level hacking
	i  = 0x5F375A86 - (i>>1);               // what the fuck? 
	y  = *(f32*)&i;

	y  = y * (ThreeHalfs - (x2 * y*y));   // 1st iteration
	y  = y * (ThreeHalfs - (x2 * y*y));   // 2nd iteration, this can be removed

	return y;
}
	inline f32
SquareRoot(f32 Number)
{
	return InverseSquareRoot(Number) * Number;
}

#define Square(number) (number * number)

	inline v3
GetMatCol(m33 Matrix, u32 ColIndex)
{
	Assert(0 <= ColIndex && ColIndex < 3);
	return {Matrix.E_[0 + ColIndex],
					Matrix.E_[3 + ColIndex],
					Matrix.E_[6 + ColIndex]};
}
	inline v3
GetMatRow(m33 Matrix, u32 RowIndex)
{
	Assert(0 <= RowIndex && RowIndex < 3);
	return {Matrix.E_[0 + 3*RowIndex],
					Matrix.E_[1 + 3*RowIndex],
					Matrix.E_[2 + 3*RowIndex]};
}

inline m33
XRotationMatrix(f32 Angle)
{
	f32 c = Cos(Angle);
	f32 s = Sin(Angle);

	m33 Result = (m33{1, 0, 0,
										0, c,-s,
										0, s, c});

	return Result;
}

inline m33
YRotationMatrix(f32 Angle)
{
	f32 c = Cos(Angle);
	f32 s = Sin(Angle);

	m33 Result = (m33{ c, 0, s,
										 0, 1, 0,
										-s, 0, c});

	return Result;
}

inline m33
ZRotationMatrix(f32 Angle)
{
	f32 c = Cos(Angle);
	f32 s = Sin(Angle);

	m33 Result = (m33{c,-s, 0,
										s, c, 0,
										0, 0, 1});

	return Result;
}

inline m33
AxisRotationMatrix(f32 Angle, v3 Axis)
{
	f32 c = Cos(Angle);
	f32 s = Sin(Angle);
	f32 t = 1.0f - c;
	f32 x = Axis.X;
	f32 y = Axis.Y;
	f32 z = Axis.Z;
	f32 xy = Axis.X*Axis.Y;
	f32 xz = Axis.X*Axis.Z;
	f32 yz = Axis.Y*Axis.Z;
	f32 xx = Square(Axis.X);
	f32 yy = Square(Axis.Y);
	f32 zz = Square(Axis.Z);

	m33 A = (m33{ c,  -s*z, s*y,
				  s*z, c,  -s*x,
				 -s*y, s*x, c});
	m33 B = (m33{xx, xy, xz,
				 xy, yy, yz,
				 xz, yz, zz} * t);
	m33 Result = A + B;

	return Result;
}

#if 0 
inline f32
ASin(f32 Value)
{
	f32 Result;

	Assert(-1.0f <= Value && Value <= 1.0f);

	f32 x2 = Value * Value;
	f32 x3 = Value * x2;
	f32 x5 = x3 * x2;
	f32 x7 = x5 * x2;
	f32 x9 = x7 * x2;

	Result = (Value + 
						x3 * (1.0f/6.0f) + 
						x5 * (3.0f/40.0f) + 
						x7 * (5.0f/112.0f) + 
						x9 * (35.0f/1152.0f));

	return Result;
}
#endif
// NOTE(bjorn): https://stackoverflow.com/questions/3380628/fast-arc-cos-algorithm
// Approximate acos(a) with relative error < 5.15e-3
// This uses an idea from Robert Harley's posting in comp.arch.arithmetic on 1996/07/12
// https://groups.google.com/forum/#!original/comp.arch.arithmetic/wqCPkCCXqWs/T9qCkHtGE2YJ
inline f32 ACos (float Value)
{
	f32 Result;

	const float C  = 0.10501094f;
	float r, s, t, u;

	t = (Value < 0) ? (-Value) : Value;  // handle negative arguments
	u = 1.0f - t;
	s = SquareRoot(u + u);

	Result = C * u * s + s;  // or fmaf (C * u, s, s) if FMA support in hardware
	if (Value < 0) Result = pi32 - Result;  // handle negative arguments

	return Result;
}

inline v3
Normalize(v3 D)
{
  return D * InverseSquareRoot(D.X*D.X + D.Y*D.Y + D.Z*D.Z);
}
inline v4
QuaternionNormalize(v4 Q)
{
	f32 d = Q.X*Q.X + Q.Y*Q.Y + Q.Z*Q.Z + Q.W*Q.W;
	if(d == 0)
	{
		return q{1,0,0,0};
	}
	else
	{
		return Q * InverseSquareRoot(d);
	}
}

inline f32
Absolute(f32 Number)
{
	if(Number < 0.0f)
	{
		return Number * -1.0f;
	}
	else
	{
		return Number;
	}
}
inline s64
AbsoluteS64(s64 Number)
{
	if(Number < 0)
	{
		return Number * -1;
	}
	else
	{
		return Number;
	}
}

	inline s32
Sign(s32 Number)
{
	if(Number == 0)
	{
		return 0;
	}
	if(Number > 0)
	{
		return 1;
	}
	return -1;
}
	inline f32
Sign(f32 Number)
{
	if(Number == 0)
	{
		return 0;
	}
	if(Number > 0)
	{
		return 1;
	}
	return -1;
}

	inline f32
Lerp(f32 t, f32 A, f32 B)
{
	return A + (B - A) * t;
}

inline f32
LengthSquared(v2 A)
{
	return Square(A.X) + Square(A.Y);
}
inline f32
LengthSquared(v3 A)
{
	return Square(A.X) + Square(A.Y) + Square(A.Z);
}

inline f32
MagnitudeSquared(v2 A)
{
	return Square(A.X) + Square(A.Y);
}
inline f32
MagnitudeSquared(v3 A)
{
	return Square(A.X) + Square(A.Y) + Square(A.Z);
}

inline f32
Length(v2 A)
{
	return SquareRoot(Square(A.X) + Square(A.Y));
}
inline f32
Length(v3 A)
{
	return SquareRoot(Square(A.X) + Square(A.Y) + Square(A.Z));
}

inline f32
Magnitude(v2 A)
{
	return SquareRoot(Square(A.X) + Square(A.Y));
}
inline f32
Magnitude(v3 A)
{
	return SquareRoot(Square(A.X) + Square(A.Y) + Square(A.Z));
}

inline f32
Distance(v2 A, v2 B)
{
	v2 D = A - B;
	return SquareRoot(D.X*D.X + D.Y*D.Y);
}

inline f32
Distance(v3 A, v3 B)
{
	v3 D = A - B;
	return SquareRoot(D.X*D.X + D.Y*D.Y + D.Z*D.Z);
}

inline b32
IsZeroVector(v3 V)
{
	return !(V.X || V.Y || V.Z);
}
inline b32
IsNonZero(v3 V)
{
	return (V.X || V.Y || V.Z);
}

inline v2
Normalize(v2 A)
{
	f32 SquaredLength = LengthSquared(A);

	if(SquaredLength > 0.0f)
	{
		f32 InverseLength = InverseSquareRoot(SquaredLength);
		A *= InverseLength;
	}

	return A;
}

inline f32
Determinant(v2 A, v2 B)
{
	return A.X*B.Y - B.X*A.Y;
}
inline f32
Determinant(m22 M)
{
	return M.A*M.D - M.B*M.C;
}
inline f32
Determinant(m33 M)
{
	return (M.A*M.E*M.I + 
					M.D*M.H*M.C + 
					M.G*M.B*M.F - 
					M.A*M.H*M.F - 
					M.G*M.E*M.C - 
					M.D*M.B*M.I);
}

inline f32 
SafeRatioN(f32 Numerator, f32 Divisor, f32 N)
{
	f32 Result = N;

	if(Divisor != 0.0f)
	{
		Result = Numerator / Divisor;
	}

	return Result;
}

inline f32 
SafeRatio0(f32 Numerator, f32 Divisor)
{
	return SafeRatioN(Numerator, Divisor, 0);
}

inline f32 
SafeRatio1(f32 Numerator, f32 Divisor)
{
	return SafeRatioN(Numerator, Divisor, 1);
}

inline f32 
Clamp(f32 Low, f32 Number, f32 High) 
{
	return (Number < Low) ? Low :((Number > High) ? High : Number);
}

inline u32 
Clamp(u32 Low, s32 Number, u32 High) 
{
	if(Number < 0) { Number = 0; }
	return ((u32)Number < Low) ? Low :(((u32)Number > High) ? High : (u32)Number);
}
		
inline f32 
Clamp01(f32 Number)
{
	return Clamp(0, Number, 1);
}
inline f32 
Clamp0(f32 Number, f32 High)
{
	return Clamp(0, Number, High);
}

inline v3 
Clamp(f32 Low, v3 Number, f32 High) 
{
	return {Clamp(Low, Number.X, High),
					Clamp(Low, Number.Y, High),
					Clamp(Low, Number.Z, High)};
}

inline v3u 
Clamp0(v3s Number, u32 High) 
{
	return {Clamp(0, Number.X, High),
					Clamp(0, Number.Y, High),
					Clamp(0, Number.Z, High)};
}

inline f32 
Modulate(f32 Value, f32 Lower, f32 Upper)
{
	Assert(Upper >= Lower);
	f32 Result = Value;

	Result -= Lower; 
	Upper -= Lower;

	if(Result > Upper)
	{
		Result -= FloorF32ToS32(Result / Upper) * Upper;
	}
	else if(Result < Lower)
	{
		//TODO(bjorn): Only Absolute here? Bug???
		Result += RoofF32ToS32(Absolute(Result) / Upper) * Upper;
	}

	Result += Lower;

	return Result;
}
inline v2 
Modulate(v2 Value, f32 Lower, f32 Upper)
{
	v2 Result = {};
	Result.X = Modulate(Value.X, Lower, Upper);
	Result.Y = Modulate(Value.Y, Lower, Upper);
	return Result;
}
inline u32 
Modulate(u32 Value, u32 Lower, u32 Upper)
{
	Assert(Upper >= Lower);
	u32 Result = Value;

	Result -= Lower;
	Upper -= Lower;

	Result -= (Result / Upper) * Upper;

	Result += Lower;

	return Result;
}

inline f32
Modulate0(f32 Value, f32 Upper)
{
	return Modulate(Value, 0, Upper);
}
inline f32
Modulate01(f32 Value)
{
	return Modulate(Value, 0, 1);
}
inline v2
Modulate0(v2 Value, f32 Upper)
{
	return Modulate(Value, 0, Upper);
}

struct rectangle2
{
	v2 Min;
	v2 Max;
};
struct rectangle2u
{
	v2u Min;
	v2u Max;
};
struct rectangle2s
{
	v2s Min;
	v2s Max;
};

struct rectangle3
{
	v3 Min;
	v3 Max;
};
struct rectangle3u
{
	v3u Min;
	v3u Max;
};
struct rectangle3s
{
	v3s Min;
	v3s Max;
};

inline rectangle2
RectMinMax(v2 Min, v2 Max)
{
	return {Min, Max};
}
inline rectangle2
RectMinDim(v2 Min, v2 Dim)
{
	return {Min, Min + Dim};
}
inline rectangle2
RectCenterHalfDim(v2 Center, v2 HalfDim)
{
	return {Center - HalfDim, Center + HalfDim};
}
inline rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
	v2 HalfDim = Dim * 0.5f;
	return {Center - HalfDim, Center + HalfDim};
}

inline rectangle2u
RectCenterDim(v2u Center, v2u Dim)
{
	v2u HalfDim = Dim / 2u;
	rectangle2u Result = {Center - HalfDim, Center + HalfDim};
	return Result;
}

inline rectangle2s
RectMinMax(v2s Min, v2s Max)
{
	return {Min, Max};
}
inline rectangle2s
RectCenterDim(v2s Center, v2u Dim)
{
	v2s HalfDim = (v2s)(Dim / 2u);
	rectangle2s Result = {Center - HalfDim, Center + HalfDim};
	return Result;
}

struct center_dim_v3_result
{
	v3 Center;
	v3 Dim;
};
inline center_dim_v3_result
RectToCenterDim(rectangle3 Rect)
{
	center_dim_v3_result Result = {};

	Result.Center = (Rect.Min + Rect.Max) * 0.5f;
	Result.Dim = Rect.Max - Rect.Min;

	return Result;
}
inline rectangle3s
RectMinMax(v3s Min, v3s Max)
{
	return {Min, Max};
}
inline rectangle3
RectMinMax(v3 Min, v3 Max)
{
	return {Min, Max};
}
inline rectangle3
RectCenterDim(v3 Center, v3 Dim)
{
	v3 HalfDim = Dim * 0.5f;
	rectangle3 Result = {Center - HalfDim, Center + HalfDim};
	return Result;
}

inline rectangle3
AddMarginToRect(rectangle3 Rect, f32 Margin)
{
	rectangle3 Result = {Rect.Min - v3{1,1,1}*Margin, Rect.Max + v3{1,1,1}*Margin};
	return Result;
}

inline b32
IsInRectangle(rectangle2 Rect, v2 P)
{
	b32 Result = ((Rect.Min.X <= P.X) &&
								(Rect.Min.Y <= P.Y) &&
								(Rect.Max.X > P.X) &&
								(Rect.Max.Y > P.Y));

	return Result;
}
inline b32
IsInRectangle(rectangle2u Rect, v2u TileP)
{
	b32 Result = ((Rect.Min.X <= TileP.X) &&
								(Rect.Min.Y <= TileP.Y) &&
								(Rect.Max.X >= TileP.X) &&
								(Rect.Max.Y >= TileP.Y));

	return Result;
}
inline b32
IsInRectangle(rectangle2s Rect, v2s TileP)
{
	b32 Result = ((Rect.Min.X <= TileP.X) &&
								(Rect.Min.Y <= TileP.Y) &&
								(Rect.Max.X >= TileP.X) &&
								(Rect.Max.Y >= TileP.Y));

	return Result;
}

inline b32
IsInRectangle(rectangle3 Rect, v3 P)
{
	b32 Result = ((Rect.Min.X <= P.X) &&
								(Rect.Min.Y <= P.Y) &&
								(Rect.Min.Z <= P.Z) &&
								(Rect.Max.X >  P.X) &&
								(Rect.Max.Y >  P.Y) &&
								(Rect.Max.Z >  P.Z));

	return Result;
}
inline b32
IsInRectangle(rectangle3u Rect, v3u TileP)
{
	b32 Result = ((Rect.Min.X <= TileP.X) &&
								(Rect.Min.Y <= TileP.Y) &&
								(Rect.Min.Z <= TileP.Z) &&
								(Rect.Max.X >= TileP.X) &&
								(Rect.Max.Y >= TileP.Y) &&
								(Rect.Max.Z >= TileP.Z));

	return Result;
}
inline b32
IsInRectangle(rectangle3s Rect, v3s TileP)
{
	b32 Result = ((Rect.Min.X <= TileP.X) &&
								(Rect.Min.Y <= TileP.Y) &&
								(Rect.Min.Z <= TileP.Z) &&
								(Rect.Max.X >= TileP.X) &&
								(Rect.Max.Y >= TileP.Y) &&
								(Rect.Max.Z >= TileP.Z));

	return Result;
}

inline q
UpdateOrientationByAngularVelocity(q O, v3 dOp)
{
	q W = q{0.0f, dOp.X, dOp.Y, dOp.Z};
	return O + (W*O)*0.5f;
}
	inline q
AngleAxisToQuaternion(f32 Angle, v3 Axis)
{
	f32 HalfAngle = Angle * 0.5f;
	f32 C = Cos(HalfAngle);
	f32 S = Sin(HalfAngle);
	return q{C, Axis.X*S, Axis.Y*S, Axis.Z*S};
}
	inline q
AngleAxisToQuaternion(v2 UnitCirclePoint, v3 Axis)
{
	f32 S = 0;
	f32 C = 0;
	if(0.5f <= UnitCirclePoint.X && UnitCirclePoint.X <= 0.5f)
	{
		f32 HalfOneMinusX_iSqrt = InverseSquareRoot((1-UnitCirclePoint.X)*0.5f);

		S = SafeRatio1(1.0f, HalfOneMinusX_iSqrt);
		C = UnitCirclePoint.Y * HalfOneMinusX_iSqrt * 0.5f;
	}
	else
	{
		f32 HalfOnePlusX_iSqrt = InverseSquareRoot((1+UnitCirclePoint.X)*0.5f);

		C = SafeRatio1(1.0f, HalfOnePlusX_iSqrt);
		S = UnitCirclePoint.Y * HalfOnePlusX_iSqrt * 0.5f;
	}
	return q{C, Axis.X*S, Axis.Y*S, Axis.Z*S};
}
inline m33
QuaternionToRotationMatrix(q Q)
{
	f32 _2ic = 2*Q.i*Q.c;
	f32 _2ij = 2*Q.i*Q.j;
	f32 _2ik = 2*Q.i*Q.k;
	f32 _2jc = 2*Q.j*Q.c;
	f32 _2jk = 2*Q.j*Q.k;
	f32 _2kc = 2*Q.k*Q.c;
	f32 _2ii = 2*Square(Q.i);
	f32 _2jj = 2*Square(Q.j);
	f32 _2kk = 2*Square(Q.k);

	return {1-(_2jj+_2kk),   _2ij-_2kc,     _2ik+_2jc,
						 _2ij+_2kc, 1-(_2ii+_2kk),    _2jk-_2ic,
						 _2ik-_2jc,    _2jk+_2ic,  1-(_2ii+_2jj)};
};

inline m44
ConstructTransform(v3 P, q O, f32 S)
{
	m33 RotMat = QuaternionToRotationMatrix(O);
	m33 ScaleMat = {S,  0,   0,
									0,  S,   0,
									0,    0, S};
	m33 M = RotMat * ScaleMat;

	return {M.A, M.B, M.C, P.X,
					M.D, M.E, M.F, P.Y,
					M.G, M.H, M.I, P.Z,
					  0,   0,   0,   1};
}
inline m44
ConstructTransform(v3 P, q O, v3 S)
{
	m33 RotMat = QuaternionToRotationMatrix(O);
	m33 ScaleMat = {S.X,  0,   0,
									0,  S.Y,   0,
									0,    0, S.Z};
	m33 M = RotMat * ScaleMat;

	return {M.A, M.B, M.C, P.X,
					M.D, M.E, M.F, P.Y,
					M.G, M.H, M.I, P.Z,
					  0,   0,   0,   1};
}
inline m44
ConstructTransform(v3 P, q O)
{
	m33 M = QuaternionToRotationMatrix(O);

	return {M.A, M.B, M.C, P.X,
					M.D, M.E, M.F, P.Y,
					M.G, M.H, M.I, P.Z,
					  0,   0,   0,   1};
}
inline m44
ConstructTransform(v3 P, m33 M)
{
	return {M.A, M.B, M.C, P.X,
					M.D, M.E, M.F, P.Y,
					M.G, M.H, M.I, P.Z,
					  0,   0,   0,   1};
}

struct inverse_m33_result
{
	m33 M;
	b32 Valid;
};

inline inverse_m33_result
InverseMatrix(m33 M)
{
	inverse_m33_result Result;
	Result.Valid = false;

	f32 Det = Determinant(M);
	if(Det)
	{
		Result.Valid = true;

		f32 iDet = 1.0f/Det;
		Result.M.A = (M.E*M.I - M.F*M.H) * iDet;
		Result.M.B = (M.C*M.H - M.B*M.I) * iDet;
		Result.M.C = (M.B*M.F - M.C*M.E) * iDet;

		Result.M.D = (M.F*M.G - M.D*M.I) * iDet;
		Result.M.E = (M.A*M.I - M.C*M.G) * iDet;
		Result.M.F = (M.C*M.D - M.A*M.F) * iDet;

		Result.M.G = (M.D*M.H - M.E*M.G) * iDet;
		Result.M.H = (M.B*M.G - M.A*M.H) * iDet;
		Result.M.I = (M.A*M.E - M.B*M.D) * iDet;

#if 0 //HANDMADE_SLOW
		if(Result.Valid)
		{
			f32 e = 0.00001f;
			m33 tM = (M * Result.M) - M33Identity();
			for(s32 i = 0;
					i < ArrayCount(tM.E_);
					i++)
			{
				Assert(-e < tM.E_[i] && tM.E_[i] < e);
			}
		}
#endif
	}

	return Result;
}

inline m33
InverseTransform(m33 T)
{
	v2 X  = { T.E_[0],  T.E_[3]};
	v2 Y  = { T.E_[1],  T.E_[4]};
	v2 mT = {-T.E_[2], -T.E_[5]};

  //TODO(bjorn): Do a valid result-thing here?
	X *= SafeRatio0(1.0f, MagnitudeSquared(X));
	Y *= SafeRatio0(1.0f, MagnitudeSquared(Y));

	m33 Result {X.E_[0], X.E_[1], Dot(mT, X),
					    Y.E_[0], Y.E_[1], Dot(mT, Y),
						    	  0,			 0,				  1};

#if 0//HANDMADE_SLOW
	//TODO(bjorn): What is a good epsilon to test here?
	f32 e = 0.001f;
	m44 tM = (T * Result) - M44Identity();
	for(s32 i = 0;
			i < ArrayCount(tM.E_);
			i++)
	{
		Assert(-e < tM.E_[i] && tM.E_[i] < e);
	}
#endif

	return Result;
}

inline m44
InverseTransform(m44 T)
{
	v3 X  = { T.E_[0],  T.E_[4],  T.E_[ 8]};
	v3 Y  = { T.E_[1],  T.E_[5],  T.E_[ 9]};
	v3 Z  = { T.E_[2],  T.E_[6],  T.E_[10]};
	v3 mT = {-T.E_[3], -T.E_[7], -T.E_[11]};

	X *= SafeRatio0(1.0f, MagnitudeSquared(X));
	Y *= SafeRatio0(1.0f, MagnitudeSquared(Y));
	Z *= SafeRatio0(1.0f, MagnitudeSquared(Z));

	m44 Result {X.E_[0], X.E_[1], X.E_[2], Dot(mT, X),
					    Y.E_[0], Y.E_[1], Y.E_[2], Dot(mT, Y),
					    Z.E_[0], Z.E_[1], Z.E_[2], Dot(mT, Z),
						    	  0,			 0,			  0,				  1};

#if 0//HANDMADE_SLOW
	//TODO(bjorn): What is a good epsilon to test here?
	f32 e = 0.001f;
	m44 tM = (T * Result) - M44Identity();
	for(s32 i = 0;
			i < ArrayCount(tM.E_);
			i++)
	{
		Assert(-e < tM.E_[i] && tM.E_[i] < e);
	}
#endif

	return Result;
}

inline m44
InverseUnscaledTransform(m44 T)
{
	v3 X  = { T.E_[0],  T.E_[4],  T.E_[ 8]};
	v3 Y  = { T.E_[1],  T.E_[5],  T.E_[ 9]};
	v3 Z  = { T.E_[2],  T.E_[6],  T.E_[10]};
	v3 mT = {-T.E_[3], -T.E_[7], -T.E_[11]};

	m44 Result {X.E_[0], X.E_[1], X.E_[2], Dot(mT, X),
					    Y.E_[0], Y.E_[1], Y.E_[2], Dot(mT, Y),
					    Z.E_[0], Z.E_[1], Z.E_[2], Dot(mT, Z),
						    	  0,			 0,			  0,				  1};

#if 0//HANDMADE_SLOW
	//TODO(bjorn): What is a good epsilon to test here?
	f32 e = 0.001f;
	m44 tM = (T * Result) - M44Identity();
	for(s32 i = 0;
			i < ArrayCount(tM.E_);
			i++)
	{
		Assert(-e < tM.E_[i] && tM.E_[i] < e);
	}
#endif

	return Result;
}

	inline m33
Transpose(m33 M)
{
	m33 Result = {};

	for(u32 i = 0;
			i < 9;
			i++)
	{
		u32 x = i % 3;
		u32 y = i / 3;
		Result.E_[x*3 + y] = M.E_[y*3 + x];
	}

	return Result;
}

	inline b32
IsWithin(f32 Value, f32 Lower, f32 Upper)
{
	return Lower < Value && Value < Upper;
}
	inline b32
IsWithinInclusive(f32 Value, f32 Lower, f32 Upper)
{
	return Lower <= Value && Value <= Upper;
}

#define MATH_H
#endif
