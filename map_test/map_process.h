#pragma once
//
//  map_process.h
// 
//  Save and process the basic parameters of the map
//

#include "tilts.h"
#include "pos_transform.h"

namespace map {
    //
    //                         longitude             * latitude
    //  Mercator projection:           x [   0,   1] *        y [  0,  1]
    //  Spherical coordinate:     lambda [-180, 180] *      phi [ 90,-90]
    //

    // The base class about the map's coordinate operations and transformations
    class Map {
    public:
        // Longitude & latitude in Mercato projection
        double lng, lat;
        // Scaling factor
        double k;
        // Tilt's zooming level
        size_t z;
        // Width & height of the display region
        size_t w, h;
        // Useful factors
        size_t tilts_per_side;
        double pixels_per_side;

        std::tuple<double, double> cursor_mercator(int mx, int my) const {
            return { lng + mx / pixels_per_side, lat + my / pixels_per_side };
        }

    public:
        // Limit the visible area
        void pos_correction() {
            double screen_height_factor = h / pixels_per_side;
            if (lng > 1) {
                lng -= 1;
            } else if (lng < 0) {
                lng += 1;
            }
            if (lat < 0.15) {
                lat = 0.15 + 1e-8;
            } else if (lat + screen_height_factor > 0.85) {
                lat = 0.85 - screen_height_factor - 1e-8;
            }
        }

        // Panning by pixels
        void translate(int dx, int dy) {
            lng -= dx / pixels_per_side;
            lat -= dy / pixels_per_side;

            pos_correction();
        }

        // Positioning the view centre
        void focus_on(double cx, double cy) {
            double dx = Map::w / pixels_per_side / 2, dy = Map::h / pixels_per_side / 2;
            lng = cx - dx, lat = cy - dy;
            pos_correction();
        }

        // Zoom the view by a factor, centred on the mouse
        bool scale(int mx, int my, double factor) {
            // Cursor's location
            auto [tx, ty] = cursor_mercator(mx, my);

            double k1 = k * factor;
            // Scaling range limit: [3, 18]
            if (k1 >= 2) {
                if (z >= 18) {
                    return false;
                }
                z += 1;
                k = k1 / 2;
            } else if (k1 < 1) {
                if (z <= 3) {
                    return false;
                }
                z -= 1;
                k = k1 * 2;
            } else {
                k = k1;
            }

            // Cursor-centered scaling
            lng += (tx - lng) * (1 - 1 / factor);
            lat += (ty - lat) * (1 - 1 / factor);

            // Re-calculating factors
            tilts_per_side = 1 << z;
            pixels_per_side = k * tilts_per_side * tilts::TILT_SIZE;

            pos_correction();
            return true;
        }

        // Synchronise current map with another, aligning zoom and position
        void sync_with(const Map& other) {
            lng = other.lng, lat = other.lat;
            k = other.k;
            z = other.z;
            tilts_per_side = other.tilts_per_side;
            pixels_per_side = other.pixels_per_side;
        }

        static std::tuple<double, double> sphere_to_mercator(double lambda, double phi) {
            auto [lambda1, phi1] = CT::wgs_to_gcj(lambda, phi);
            double x = (lambda1 + 180.0) / 360.0;
            double y = (0.5 - std::log(std::tan(phi1 * M_PI / 180) +
                1 / std::cos(phi1 * M_PI / 180)) /
                (2 * M_PI));
            return { x, y };
        }

        static std::tuple<double, double> mercator_to_sphere(double x, double y) {
            double lambda = x * 360.0 - 180.0;
            double phi = std::atan(std::sinh(M_PI - 2 * M_PI * y)) * 180.0 / M_PI;
            return CT::gcj_to_wgs(lambda, phi);
        }

        static tilts::TiltId mercator_to_tilt_id(double x, double y, int z) {
            int i = int(x * (1 << z));
            int j = int(y * (1 << z));
            return tilts::TiltId{ .x = i, .y = j, .z = z };
        }

        Map(size_t _w, size_t _h, double _k, size_t _z) : w(_w), h(_h), k(_k), z(_z) {
            //auto sjtu = std::apply(CT::wgs_to_gcj, sphere_to_mercator(121.417, 31.042));
            //auto sjtu = std::apply(CT::wgs_to_gcj, sphere_to_mercator(-10, 10));
            //lng = get<0>(sjtu), lat = get<1>(sjtu);
            lng = lat = 0.233;
            tilts_per_side = 1 << z;
            pixels_per_side = k * tilts_per_side * tilts::TILT_SIZE;
            focus_on(0.837324, 0.409268);
        }
    };
}