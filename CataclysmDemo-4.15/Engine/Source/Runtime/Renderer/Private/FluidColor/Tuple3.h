#pragma once

#include <math.h>
#include <algorithm>
using std::max;
using std::min;


template <typename Real>
class Tuple3
{
	Real _m[3];

public:
	explicit Tuple3(Real s = Real()) { _m[0] = _m[1] = _m[2] = s; }
	Tuple3(Real m0, Real m1, Real m2) { _m[0] = m0; _m[1] = m1; _m[2] = m2; }
	Tuple3(const Real* m) { _m[0] = m[0]; _m[1] = m[1]; _m[2] = m[2]; }

	Real& operator[] (int i) { return _m[i]; }
	Real operator[] (int i) const { return _m[i]; }

	Real* as_array() { return _m; }
	const Real* as_array() const { return _m; }


	friend Tuple3 operator- (const Tuple3& t) {
		return Tuple3(-t._m[0], -t._m[1], -t._m[2]);
	}

	Tuple3& operator+= (const Tuple3& t) {
		_m[0] += t._m[0]; _m[1] += t._m[1]; _m[2] += t._m[2];
		return *this;
	}
	Tuple3& operator-= (const Tuple3& t) {
		_m[0] -= t._m[0]; _m[1] -= t._m[1]; _m[2] -= t._m[2];
		return *this;
	}
	Tuple3& operator*= (Real s) {
		_m[0] *= s; _m[1] *= s; _m[2] *= s;
		return *this;
	}
	Tuple3& operator/= (Real s) {
		_m[0] /= s; _m[1] /= s; _m[2] /= s;
		return *this;
	}

	friend Tuple3 operator* (Real s, const Tuple3& tr) {
		return Tuple3(s * tr._m[0], s * tr._m[1], s * tr._m[2]);
	}
	friend Tuple3 operator* (const Tuple3& tl, Real s) {
		return Tuple3(tl._m[0] * s, tl._m[1] * s, tl._m[2] * s);
	}
	friend Tuple3 operator/ (const Tuple3& tl, Real s) {
		return tl * (1.0 / s);
	}

	friend Tuple3 operator* (const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(tl._m[0] * tr._m[0], tl._m[1] * tr._m[1], tl._m[2] * tr._m[2]);
	}
	friend Tuple3 operator/ (const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(tl._m[0] / tr._m[0], tl._m[1] / tr._m[1], tl._m[2] / tr._m[2]);
	}

	friend Tuple3 operator+ (const Tuple3& tl, Real s) {
		return Tuple3(tl._m[0] + s, tl._m[1] + s, tl._m[2] + s);
	}
	friend Tuple3 operator+ (Real s, const Tuple3& tr) {
		return Tuple3(s + tr._m[0], s + tr._m[1], s + tr._m[2]);
	}
	friend Tuple3 operator+ (const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(tl._m[0] + tr._m[0], tl._m[1] + tr._m[1], tl._m[2] + tr._m[2]);
	}

	friend Tuple3 operator- (const Tuple3& tl, Real s) {
		return Tuple3(tl._m[0] - s, tl._m[1] - s, tl._m[2] - s);
	}
	friend Tuple3 operator- (Real s, const Tuple3& tr) {
		return Tuple3(s - tr._m[0], s - tr._m[1], s - tr._m[2]);
	}
	friend Tuple3 operator- (const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(tl._m[0] - tr._m[0], tl._m[1] - tr._m[1], tl._m[2] - tr._m[2]);
	}

	friend Tuple3 min(const Tuple3& tl, Real s) {
		return Tuple3(min(tl._m[0], s), min(tl._m[1], s), min(tl._m[2], s));
	}
	friend Tuple3 min(const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(min(tl._m[0], tr._m[0]), min(tl._m[1], tr._m[1]), min(tl._m[2], tr._m[2]));
	}

	friend Tuple3 max(const Tuple3& tl, Real s) {
		return Tuple3(max(tl._m[0], s), max(tl._m[1], s), max(tl._m[2], s));
	}
	friend Tuple3 max(const Tuple3& tl, const Tuple3& tr) {
		return Tuple3(max(tl._m[0], tr._m[0]), max(tl._m[1], tr._m[1]), max(tl._m[2], tr._m[2]));
	}

	friend Real dot(const Tuple3& tl, const Tuple3& tr)	{
		return tl._m[0] * tr._m[0] + tl._m[1] * tr._m[1] + tl._m[2] * tr._m[2];
	}

	friend Tuple3 exp(const Tuple3& t) {
		return Tuple3(exp(t._m[0]), exp(t._m[1]), exp(t._m[2]));
	}
};

typedef Tuple3<float> Tuple3f;
typedef Tuple3<double> Tuple3d;
