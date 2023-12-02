#pragma once
//
//  pos_transform.h
// 
//  Transform coordinate between earth(WGS-84) and mars in china(GCJ-02).
//  ref - https://github.com/googollee/eviltransform/
//

#define M_PI 3.14159265358979323846

namespace CT {
#define EARTH_R 6378245.0

	inline static bool out_of_China(double lat, double lng) {
		return (lng < 72.004 || lng > 137.8347 || lat < 0.8293 || lat > 55.8271);
	}

	std::tuple<double, double> transform(double x, double y) {
		double xy = x * y;
		double absX = sqrt(fabs(x));
		double xPi = x * M_PI;
		double yPi = y * M_PI;
		double d = 20.0 * sin(6.0 * xPi) + 20.0 * sin(2.0 * xPi);

		double lat = d;
		double lng = d;

		lat += 20.0 * sin(yPi) + 40.0 * sin(yPi / 3.0);
		lng += 20.0 * sin(xPi) + 40.0 * sin(xPi / 3.0);

		lat += 160.0 * sin(yPi / 12.0) + 320 * sin(yPi / 30.0);
		lng += 150.0 * sin(xPi / 12.0) + 300.0 * sin(xPi / 30.0);

		lat *= 2.0 / 3.0;
		lng *= 2.0 / 3.0;

		lat += -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * xy + 0.2 * absX;
		lng += 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * xy + 0.1 * absX;
		return { lat, lng };
	}

	static std::tuple<double, double> delta(double lat, double lng) {
		const double ee = 0.00669342162296594323;
		auto [dLat, dLng] = transform(lng - 105.0, lat - 35.0);
		double radLat = lat / 180.0 * M_PI;
		double magic = sin(radLat);
		magic = 1 - ee * magic * magic;
		double sqrtMagic = sqrt(magic);
		dLat = (dLat * 180.0) / ((EARTH_R * (1 - ee)) / (magic * sqrtMagic) * M_PI);
		dLng = (dLng * 180.0) / (EARTH_R / sqrtMagic * cos(radLat) * M_PI);
		return { dLat, dLng };
	}

	std::tuple<double, double> wgs_to_gcj(double wgsLng, double wgsLat) {
		if (out_of_China(wgsLat, wgsLng)) {
			return { wgsLng, wgsLat };
		}
		auto [dLat, dLng] = delta(wgsLat, wgsLng);
		return { wgsLng + dLng, wgsLat + dLat };
	}

	std::tuple<double, double> gcj_to_wgs(double gcjLng, double gcjLat) {
		if (out_of_China(gcjLat, gcjLng)) {
			return { gcjLng, gcjLat };
		}
		auto [dLat, dLng] = delta(gcjLat, gcjLng);
		return { gcjLng - dLng, gcjLat - dLat };
	}

	std::tuple<double, double> gcj_to_wgs_exact(double gcjLng, double gcjLat) {
		double initDelta = 0.01;
		double threshold = 0.000001;
		double dLat, dLng, mLat, mLng, pLat, pLng, wgsLat, wgsLng;
		dLat = dLng = initDelta;
		mLat = gcjLat - dLat;
		mLng = gcjLng - dLng;
		pLat = gcjLat + dLat;
		pLng = gcjLng + dLng;
		int i;
		for (i = 0; i < 30; i++) {
			wgsLat = (mLat + pLat) / 2;
			wgsLng = (mLng + pLng) / 2;
			auto [tmpLng, tmpLat] = wgs_to_gcj(wgsLng, wgsLat);
			dLat = tmpLat - gcjLat;
			dLng = tmpLng - gcjLng;
			if ((fabs(dLat) < threshold) && (fabs(dLng) < threshold)) {
				break;
			}
			if (dLat > 0) {
				pLat = wgsLat;
			} else {
				mLat = wgsLat;
			}
			if (dLng > 0) {
				pLng = wgsLng;
			} else {
				mLng = wgsLng;
			}
		}
		return { wgsLng, wgsLat };
	}
} // namespace CT