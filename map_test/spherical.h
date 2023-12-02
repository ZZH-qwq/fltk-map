#pragma once
//
//  spherical.h
// 
//  Provides tools for spherical geometry
//

namespace sphere {

	// Spherical distance and azimuth formula
	// ref - https://en.wikipedia.org/wiki/Haversine_formula
	//     - http://www.movable-type.co.uk/scripts/latlong.html

	long double distance(double latA, double lngA, double latB, double lngB) {
		long double arcLatA = latA * M_PI / 180;
		long double arcLatB = latB * M_PI / 180;
		long double x = cos(arcLatA) * cos(arcLatB) * cos((lngA - lngB) * M_PI / 180);
		long double y = sin(arcLatA) * sin(arcLatB);
		long double s = x + y;
		if (s > 1) {
			s = 1;
		} else if (s < -1) {
			s = -1;
		}
		long double alpha = acos(s);
		return alpha * EARTH_R;
	}

	long double initial_bearing(double latA, double lngA, double latB, double lngB) {
		long double arcLatA = latA * M_PI / 180;
		long double arcLatB = latB * M_PI / 180;
		long double arcdLng = (lngB - lngA) * M_PI / 180;
		return atan2(sin(arcdLng) * cos(arcLatB), cos(arcLatA) * sin(arcLatB) - sin(arcLatA) * cos(arcLatB) * cos(arcdLng));
	}

	long double spherical_triangle(double latA, double lngA, double latB, double lngB, double latC, double lngC) {
		if (latA == latB && lngA == lngB || latA == latC && lngA == lngC || latB == latC && lngB == lngC) {
			return 0;
		}
		long double a = distance(latB, lngB, latC, lngC), b = distance(latA, lngA, latC, lngC), c = distance(latA, lngA, latB, lngB);
		long double A = initial_bearing(latA, lngA, latB, lngB) - initial_bearing(latA, lngA, latC, lngC);
		long double B = initial_bearing(latB, lngB, latC, lngC) - initial_bearing(latB, lngB, latA, lngA);
		long double C = initial_bearing(latC, lngC, latA, lngA) - initial_bearing(latC, lngC, latB, lngB);

		if (a <= 1e2 || b <= 1e2 || c <= 1e2) {
			long double p = (a + b + c) / 2;
			bool f = (A + B + C + (A + B + C > 0 ? -M_PI : M_PI)) > 0;
			//std::cout << f << " " << a << " " << b << " " << c << std::endl;
			return (f ? 1 : -1) * sqrt(p * (p - a) * (p - b) * (p - c));
		}
		if (A + B + C > 0) {
			return (A + B + C - M_PI) * EARTH_R * EARTH_R;
		} else {
			return (A + B + C + M_PI) * EARTH_R * EARTH_R;
		}
	}
}