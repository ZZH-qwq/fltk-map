#include "httplib.h"
#include <FL/Enumerations.H>
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

#include <cmath>
#include <algorithm>
#include <tuple>
#include <thread>
#include <optional>
#include <variant>
#include <random>
#include <string>
#include <map>
#include <list>
#include <sstream>
#include <iostream>
#include <iomanip>

#define DEBUG true
// #define NO_MAP 1

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
    return {lat, lng};
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
    return {dLat, dLng};
}

std::tuple<double, double> wgs_to_gcj(double wgsLng, double wgsLat) {
    if (out_of_China(wgsLat, wgsLng)) {
        return {wgsLng, wgsLat};
    }
    auto [dLat, dLng] = delta(wgsLat, wgsLng);
    return {wgsLng + dLng, wgsLat + dLat};
}

std::tuple<double, double> gcj_to_wgs(double gcjLng, double gcjLat) {
    if (out_of_China(gcjLat, gcjLng)) {
        return {gcjLng, gcjLat};
    }
    auto [dLat, dLng] = delta(gcjLat, gcjLng);
    return {gcjLng - dLng, gcjLat - dLat};
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
    return {wgsLng, wgsLat};
}
}  // namespace CT

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
        // std::cout << f << " " << a << " " << b << " " << c << std::endl;
        return (f ? 1 : -1) * sqrt(p * (p - a) * (p - b) * (p - c));
    }
    if (A + B + C > 0) {
        return (A + B + C - M_PI) * EARTH_R * EARTH_R;
    } else {
        return (A + B + C + M_PI) * EARTH_R * EARTH_R;
    }
}
}  // namespace sphere

namespace area {
#define EPSILON 1e-16

// 2D vec of double
struct Vec2d {
    double x, y;
    Vec2d() { x = 0.0, y = 0.0; }
    Vec2d(double dx, double dy) { x = dx, y = dy; }
    void Set(double dx, double dy) { x = dx, y = dy; }
};

// Determine if a point is on a line segment
bool is_point_on_line(const Vec2d q, const Vec2d p1, const Vec2d p2) {
    double d1 = (p1.x - q.x) * (p2.y - q.y) - (p2.x - q.x) * (p1.y - q.y);
    return ((abs(d1) < EPSILON) && ((q.x - p1.x) * (q.x - p2.x) <= 0) && ((q.y - p1.y) * (q.y - p2.y) <= 0));
}

// Determine whether a line segments and a x-aligned ray intersect
// Assert the ray, start at q1, is parallel to the x-axis and towards the direction of x+
// If so, give out the point of intersection
std::optional<double> ray_intersect(const Vec2d& p1, const Vec2d& p2, const Vec2d& q1) {
    if (std::max(p1.x, p2.x) < q1.x || std::max(p1.y, p2.y) < q1.y || q1.y < std::min(p1.y, p2.y)) {
        return std::nullopt;
    }
    if (is_point_on_line(q1, p1, p2) || abs(p2.y - p1.y) < EPSILON) {
        return std::nullopt;
    }
    // Only the upper endpoint counts
    if (abs(p1.y - q1.y) <= EPSILON) {
        if (p1.x > q1.x && p1.y > p2.y) {
            return p1.x;
        } else {
            return std::nullopt;
        }
    }
    if (abs(p2.y - q1.y) <= EPSILON) {
        if (p2.x > q1.x && p2.y > p1.y) {
            return p2.x;
        } else {
            return std::nullopt;
        }
    }

    double p1q = p1.y - q1.y;
    double qp2 = q1.y - p2.y;
    double x = p1.x + (p2.x - p1.x) * (p1q / (p1q + qp2));
    if (x < q1.x) {
        return std::nullopt;
    } else {
        return x;
    }
}

// Determine whether two line segments intersect
bool is_intersect(const Vec2d& p1, const Vec2d& p2, const Vec2d& q1, const Vec2d& q2) {
    if (std::max(p1.x, p2.x) < std::min(q1.x, q2.x) || std::max(p1.y, p2.y) < std::min(q1.y, q2.y) ||
        std::max(q1.x, q2.x) < std::min(p1.x, p2.x) || std::max(q1.y, q2.y) < std::min(p1.y, p2.y)) {
        return false;
    }
    if (((q1.x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (q1.y - p1.y)) *
                ((q2.x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (q2.y - p1.y)) >=
            0 ||
        ((p1.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p1.y - q1.y)) *
                ((p2.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p2.y - q1.y)) >=
            0) {
        return false;
    }
    return true;
}

double hsl_val(double n1, double n2, double hue) {
    if (hue > 360) {
        hue -= 360;
    } else if (hue < 0) {
        hue += 360;
    }
    if (hue < 60) {
        return n1 + (n2 - n1) * hue / 60;
    } else if (hue < 180) {
        return n2;
    } else if (hue < 240) {
        return n1 + (n2 - n1) * (240 - hue) / 60;
    } else {
        return n1;
    }
}

std::tuple<uchar, uchar, uchar> hsl_to_rgb(double h, double s, double l) {
    double cmax = l <= 0.5 ? l * (1 + s) : l * (1 - s) + s, cmin = 2 * l - cmax;
    return {hsl_val(cmin, cmax, h + 120) * 255, hsl_val(cmin, cmax, h) * 255, hsl_val(cmin, cmax, h - 120) * 255};
}

std::tuple<uchar, uchar, uchar> color_generator() {
    static std::default_random_engine e;
    static int hue = 0;
    std::uniform_real_distribution<double> h(180, 300), l(0.45, 0.75), s(0.75, 1);
    hue = int(hue + h(e)) % 360;
    return hsl_to_rgb(hue, s(e), l(e));
}
}  // namespace area

namespace tilts {
const int TILT_SIZE = 256;

class TiltId {
   public:
    int x, y, z;

    const char* UDT = "20231025";

    std::string to_request_url() {
        std::stringstream ss;
        ss << "/appmaptile?lang=zh_cn&size=1&scale=1&style=" << (z < 13 ? "8" : "7") << "&x=" << x << "&y=" << y
           << "&z=" << z;
        return ss.str();
    }

    bool operator==(const TiltId& rhs) const {
        return z == rhs.z && x == rhs.x && y == rhs.y;
    }
    bool operator<(const TiltId& rhs) const {
        return (z < rhs.z) || (z == rhs.z && x < rhs.x) ||
               (z == rhs.z && x == rhs.x && y < rhs.y);
    }

    TiltId offset(int i, int j) { return TiltId{.x = x + i, .y = y + j, .z = z}; }

    friend std::ostream& operator<<(std::ostream& os, TiltId id) {
        os << "(" << id.x << ", " << id.y << ", " << id.z << ")";
        return os;
    }
};

struct TiltData {
    const unsigned char* buf;
    size_t size;

   public:
    TiltData() : size(0), buf(nullptr) {}
    TiltData(const unsigned char* buf, size_t size) : size(size), buf(buf) {}
};

struct TiltFuture {
    std::atomic_bool available;
    TiltId id;
    int status;
    std::string data;

    TiltFuture(TiltId id) : available(false), id(id) {}

    static void makeRequest(httplib::Client* cli, std::string url,
                            TiltFuture* future) {
        if (auto res = cli->Get(url)) {
            if (res->status == 200) {
                future->data = std::move(res->body);
            }
            future->status = res->status;
        } else {
            future->status = -1;
        }
        future->available.store(true);
    }
};

class TiltsSource {
    std::map<TiltId, std::string> tilts;
    std::list<TiltId> cached;
    std::list<TiltFuture*> futures;
    std::set<TiltId> downloading;
    int size;
    httplib::Client cli;

   public:
    TiltsSource(int size) : cli("http://webrd03.is.autonavi.com"), size(size) {}

    bool cacheHas(TiltId id) { return tilts.find(id) != tilts.end(); }
    bool isDownloading(TiltId id) {
        return downloading.find(id) != downloading.end();
    }

    void download(TiltId id) {
        if (cacheHas(id) || isDownloading(id)) {
            return;
        }

        auto future = new TiltFuture(id);
        std::thread th(TiltFuture::makeRequest, &cli, id.to_request_url(), future);
        th.detach();
        futures.push_back(future);
        downloading.insert(id);
    }

    std::tuple<bool, bool> pollFutures() {
        if (futures.empty()) {
            return {false, false};
        }
        auto front = futures.front();
        futures.pop_front();

        auto oldFuturesCnt = futures.size();
        auto updated = false;
        if (front->available.load()) {
            if (front->status == 200) {
                while (cached.size() >= size) {
                    auto idPop = cached.front();
                    cached.pop_front();
                    tilts.erase(idPop);
                }

                cached.push_back(front->id);
                tilts[front->id] = front->data;
                downloading.erase(front->id);
                updated = true;

                if (DEBUG)
                    std::cout << "Downloaded tilt " << front->id << std::endl;

                // invalid status code
            } else {
                if (DEBUG)
                    std::cout << "Download tilt " << front->id
                              << " failed with status code" << front->status << std::endl;
            }

            delete front;

            // unavailable
        } else {
            futures.push_back(front);
        }

        return {oldFuturesCnt < futures.size(), updated};
    }

    TiltData get(TiltId id) {
        download(id);
        pollFutures();

        if (cacheHas(id)) {
            return TiltData((const unsigned char*)tilts[id].c_str(),
                            tilts[id].size());
        } else {
            return TiltData();
        }
    }
};
}  // namespace tilts

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
        return {lng + mx / pixels_per_side, lat + my / pixels_per_side};
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
        return {x, y};
    }

    static std::tuple<double, double> mercator_to_sphere(double x, double y) {
        double lambda = x * 360.0 - 180.0;
        double phi = std::atan(std::sinh(M_PI - 2 * M_PI * y)) * 180.0 / M_PI;
        return CT::gcj_to_wgs(lambda, phi);
    }

    static tilts::TiltId mercator_to_tilt_id(double x, double y, int z) {
        int i = int(x * (1 << z));
        int j = int(y * (1 << z));
        return tilts::TiltId{.x = i, .y = j, .z = z};
    }

    Map(size_t _w, size_t _h, double _k, size_t _z) : w(_w), h(_h), k(_k), z(_z) {
        // auto sjtu = std::apply(CT::wgs_to_gcj, sphere_to_mercator(121.417, 31.042));
        // auto sjtu = std::apply(CT::wgs_to_gcj, sphere_to_mercator(-10, 10));
        // lng = get<0>(sjtu), lat = get<1>(sjtu);
        lng = lat = 0.233;
        tilts_per_side = 1 << z;
        pixels_per_side = k * tilts_per_side * tilts::TILT_SIZE;
        focus_on(0.837324, 0.409268);
    }
};
}  // namespace map

namespace area {

// Class of polygonal areas on map
class Area {
   protected:
    // Stored data [R,G,B,A] and size of the image to be drawn
    uchar* img_data;
    size_t data_size;
    Fl_Image* image;
    size_t display_w, display_h, img_w, img_h;

    // Points of the border
    std::vector<Vec2d> polygon;
    // Bounding box
    Vec2d bbox1, bbox2;
    // Stored color
    uchar cR, cG, cB, cA;
    // Anchor dropped when image is ready to reuse
    Vec2d anchor;
    bool display = true;

    long double area_size = 0;
    Vec2d temp_point;
    std::string tag;

   public:
    void push(double x, double y) {
        if (polygon.empty()) {
            // Initialization
            bbox1 = {x, y};
            bbox2 = {x, y};
            temp_point = {x, y};
        } else {
            // Update bounding box
            bbox1 = {std::min(bbox1.x, x), std::min(bbox1.y, y)};
            bbox2 = {std::max(bbox2.x, x), std::max(bbox2.y, y)};
        }
        // Push back to point set
        polygon.push_back({x, y});

        if (polygon.size() > 2) {
            // Update area's size
            auto &a = polygon.front(), &b = *(polygon.rbegin() + 1), &c = *polygon.rbegin();
            auto [xa, ya] = map::Map::mercator_to_sphere(a.x, a.y);
            auto [xb, yb] = map::Map::mercator_to_sphere(b.x, b.y);
            auto [xc, yc] = map::Map::mercator_to_sphere(c.x, c.y);
            area_size += sphere::spherical_triangle(ya, xa, yb, xb, yc, xc);
            if (DEBUG) {
                std::cout << "Size added : " << sphere::spherical_triangle(ya, xa, yb, xb, yc, xc) << " now : " << area_size << std::endl;
            }
        }
    }

    // Return if area's bounding box is out of screen
    bool is_clipped(double x1, double y1, double x2, double y2) const {
        return (x1 > bbox2.x || y1 > bbox2.y || x2 < bbox1.x || y2 < bbox1.y);
    }
    // Return if area's bounding box is smaller than the screen
    bool is_fit(double dx, double dy) const {
        return (dx > bbox2.x - bbox1.x && dy > bbox2.y - bbox1.y);
    }

    // Fill area with its color
    void fill(double x1, double y1, double x2, double y2, bool resize = true, bool has_temp = false) {
        if (polygon.size() < 2 || polygon.size() < 3 && !has_temp) {
            return;
        }
        double dx = x2 - x1, dy = y2 - y1;
        if (resize) {
            // Reset the anchor
            anchor = {0, 0};
        } else if (anchor.y > EPSILON) {
            // Have anchor - draw image in relative posision
            image->draw((anchor.x - x1) / dx * display_w, (anchor.y - y1) / dy * display_h);
            return;
        }
        if (!has_temp && is_fit(dx, dy)) {
            generate_img(bbox1.x, bbox1.y, dx, dy, has_temp);
            image->draw((bbox1.x - x1) / dx * display_w, (bbox1.y - y1) / dy * display_h);
            if (DEBUG) {
                std::cout << "Anchor dropped" << std::endl;
            }
            // Drop the anchor
            anchor = {bbox1.x, bbox1.y};
            return;
        }
        generate_img(x1, y1, dx, dy, has_temp);
        image->draw(0, 0);
    }

    // Trace the outline of the area
    void outline(double x, double y, double scale, bool has_temp = false) const {
        if (polygon.empty()) {
            return;
        }
        fl_color(cR, cG, cB);
        if (polygon.size() < 3 || !has_temp || has_temp && legal()) {
            fl_begin_line();
            fl_line_style(FL_SOLID, 3);
            for (auto& i : polygon) {
                fl_vertex((i.x - x) * scale, (i.y - y) * scale);
            }
            fl_end_line();
            if (has_temp) {
                fl_begin_line();
                fl_line_style(FL_DASHDOT, 3);
                fl_vertex((polygon.back().x - x) * scale, (polygon.back().y - y) * scale);
                fl_vertex((temp_point.x - x) * scale, (temp_point.y - y) * scale);
                fl_end_line();
                fl_begin_line();
                fl_line_style(FL_DOT, 2);
                fl_vertex((temp_point.x - x) * scale, (temp_point.y - y) * scale);
                fl_vertex((polygon.front().x - x) * scale, (polygon.front().y - y) * scale);
                fl_end_line();
            }
        } else {
            fl_begin_loop();
            fl_line_style(FL_DOT, 3);
            for (auto& i : polygon) {
                fl_vertex((i.x - x) * scale, (i.y - y) * scale);
            }
            fl_vertex((temp_point.x - x) * scale, (temp_point.y - y) * scale);
            fl_end_loop();
        }
    }

    // Show an indicator pointing at area's center
    void indicator(double cx, double cy, size_t w, size_t h) {
        fl_color(cR, cG, cB);
        const double phi = M_PI * 5 / 18;
        auto c = center();
        double distance = sphere::distance(cx, cy, c.x, c.y);
        double theta = sphere::initial_bearing(cx, cy, c.x, c.y);
        double wx = (abs(theta) > M_PI / 2 ? -0.45 : 0.45) * w, wy = wx * tan(theta);
        double hy = (theta > 0 ? 0.45 : -0.45) * h, hx = hy / tan(theta);
        double ix, iy;
        if (abs(hx) > abs(wx) || abs(hy) > abs(wy)) {
            ix = wx, iy = wy;
        } else {
            ix = hx, iy = hy;
        }
        double angle1 = theta + phi - M_PI, angle2 = theta - phi - M_PI;
        double lmax = std::min(w, h) / 12.0, lmin = std::min(w, h) / 30.0;
        double length = lmax * pow(1.05, -distance / 3) + lmin;
        fl_line_style(FL_SOLID, length / 25 + 2);
        double sx = ix + length * cos(angle1), sy = iy + length * sin(angle1);
        double ex = ix + length * cos(angle2), ey = iy + length * sin(angle2);
        auto a = w / 2 + ix, b = h / 2 + iy;
        fl_begin_line();
        fl_vertex(w / 2 + sx, h / 2 + sy);
        fl_vertex(w / 2 + ix, h / 2 + iy);
        fl_vertex(w / 2 + ex, h / 2 + ey);
        fl_end_line();
    }

   protected:
    // Determine if the point is inside the polygon, then give the set of intersections of the ray with the polygon
    std::tuple<bool, std::vector<double>> trace_ray(double x, double y, bool has_temp = false) const {
        std::vector<double> intersects;
        Vec2d ray_start(x - 1e-15, y - 1e-15);

        // Iterate over each side of the polygon
        for (int i = 0; i < polygon.size() - 1; i++) {
            auto& c1 = polygon[i];
            auto& c2 = polygon[i + 1];

            if (auto c = ray_intersect(c1, c2, ray_start); c) {
                intersects.push_back(c.value());
            }
        }
        // Is an area under construction
        if (has_temp) {
            auto& c1 = polygon.back();
            auto& c2 = polygon.front();
            if (auto c = ray_intersect(c1, temp_point, ray_start); c) {
                intersects.push_back(c.value());
            }
            if (auto c = ray_intersect(temp_point, c2, ray_start); c) {
                intersects.push_back(c.value());
            }
        }

        return {intersects.size() % 2, intersects};
    }

    void generate_img(double x1, double y1, double dx, double dy, bool has_temp = false) {
        std::fill(img_data, img_data + data_size, 0);
        for (size_t j = 0; j < img_h; j++) {
            double y = dy * j / img_h + y1;
            // Begin status of start point and the set of intersections along the ray
            auto [st, iset] = trace_ray(x1 + dx / display_w, y + dy / display_h, has_temp);
            if (iset.empty()) {
                // No intersection
                continue;
            }
            std::sort(iset.begin(), iset.end());
            // Starting at the first filled point
            size_t i = st ? 0 : (iset[0] - x1) * img_w / dx;
            // Refers to the index of filled period's end
            int idx = st ? 0 : 1;
            for (; idx <= iset.size() && i < img_w; idx += 2) {
                size_t i_end = idx < iset.size() ? (iset[idx] - x1) * img_w / dx : img_w;
                for (; i < i_end && i < img_w; i++) {
                    // Fill pixels in filled period
                    size_t target = (j * img_w + i) * 4;
                    img_data[target] = cR;
                    img_data[target + 1] = cG;
                    img_data[target + 2] = cB;
                    img_data[target + 3] = cA;
                }
                if (idx >= iset.size() - 1 || i >= img_w) {
                    break;
                }
                i = (iset[idx + 1] - x1) * img_w / dx;
            }
        }
        delete image;
        Fl_RGB_Image img(img_data, img_w, img_h, 4);
        image = img.copy(display_w, display_h);
    }

   public:
    Area(size_t _w, size_t _h, uchar R, uchar G, uchar B, uchar A) noexcept : display_w(_w), display_h(_h), img_w(_w / 3), img_h(_h / 3), cR(R), cG(G), cB(B), cA(A) {
        data_size = img_w * img_h * 4;
        img_data = new uchar[data_size];
        image = nullptr;
    }
    Area(Area&& other) noexcept : img_data(other.img_data), data_size(other.data_size), image(other.image), display_h(other.display_h), display_w(other.display_w), img_w(other.img_w), img_h(other.img_h), polygon(std::forward<std::vector<Vec2d>&&>(other.polygon)), bbox1(other.bbox1), bbox2(other.bbox2), cR(other.cR), cG(other.cG), cB(other.cB), cA(other.cA), display(other.display), temp_point(other.temp_point), area_size(other.area_size), tag(other.tag) {
        other.img_data = nullptr;
        other.image = nullptr;
    }
    Area(Area& other) noexcept : img_data(other.img_data), data_size(other.data_size), image(other.image), display_h(other.display_h), display_w(other.display_w), img_w(other.img_w), img_h(other.img_h), polygon(other.polygon), bbox1(other.bbox1), bbox2(other.bbox2), cR(other.cR), cG(other.cG), cB(other.cB), cA(other.cA), display(other.display), temp_point(other.temp_point), area_size(other.area_size), tag(other.tag) {
        other.img_data = nullptr;
        other.image = nullptr;
    }
    ~Area() {
        delete[] img_data;
        delete image;
    }

    Vec2d center() const { return Vec2d((bbox1.x + bbox2.x) / 2, (bbox1.y + bbox2.y) / 2); }

    bool visible() const { return display; }
    void flip_visible() {
        display = !display;
        reset_anchor();
    }

    Fl_Color color() const { return cR << 24 | cG << 16 | cB << 8; }
    void color(uchar R, uchar G, uchar B, uchar A) {
        cR = R, cG = G, cB = B, cA = A;
    }

    void set_temp(double x, double y) { temp_point.x = x, temp_point.y = y; }
    void reset_temp() {
        if (!polygon.empty()) {
            temp_point = polygon.front();
        }
    }

    double size() const { return abs(area_size); }
    double temp_size() const {
        if (polygon.size() < 2) {
            return 0;
        }
        auto &a = polygon.front(), &b = *polygon.rbegin();
        auto [xa, ya] = map::Map::mercator_to_sphere(a.x, a.y);
        auto [xb, yb] = map::Map::mercator_to_sphere(b.x, b.y);
        auto [xc, yc] = map::Map::mercator_to_sphere(temp_point.x, temp_point.y);
        /*if (DEBUG) {
            std::cout << "Temp size : " << area_size + sphere::spherical_triangle(ya, xa, yb, xb, yc, xc) << std::endl;
        }*/
        return abs(area_size + sphere::spherical_triangle(ya, xa, yb, xb, yc, xc));
    }
    // Recalculate area size and bounding box, only call before polygon finished
    void recalculate() {
        area_size = 0;
        if (polygon.empty()) {
            return;
        }
        bbox1 = {polygon[0].x, polygon[0].y};
        bbox2 = {polygon[0].x, polygon[0].y};
        for (size_t i = 1; i < polygon.size(); i++) {
            bbox1 = {std::min(bbox1.x, polygon[i].x), std::min(bbox1.y, polygon[i].y)};
            bbox2 = {std::max(bbox2.x, polygon[i].x), std::max(bbox2.y, polygon[i].y)};
        }
        if (polygon.size() < 3) {
            return;
        }
        auto [p0x, p0y] = map::Map::mercator_to_sphere(polygon[0].x, polygon[0].y);
        auto [p1x, p1y] = map::Map::mercator_to_sphere(polygon[1].x, polygon[1].y);
        for (size_t i = 2; i < polygon.size(); i++) {
            auto [p2x, p2y] = map::Map::mercator_to_sphere(polygon[i].x, polygon[i].y);
            area_size += sphere::spherical_triangle(p0y, p0x, p1y, p1x, p2y, p2x);
            p1x = p2x, p1y = p2y;
        }
    }

    size_t points_count() const { return polygon.size(); }
    void reset_anchor() { anchor = {0, 0}; }

    // Check if area after adding temp_point is legal
    bool legal() const {
        if (polygon.size() < 3) {
            return true;
        }
        for (int i = 0; i < polygon.size() - 2; i++) {
            auto& c1 = polygon[i];
            auto& c2 = polygon[i + 1];
            if (is_intersect(c1, c2, polygon.back(), temp_point)) {
                return false;
            }
        }
        return true;
    }

    // Check if area size is calculable after adding temp_point
    bool size_legal() const {
        if (polygon.size() < 3) {
            return true;
        }
        if (!legal()) {
            return false;
        }
        for (int i = 1; i < polygon.size() - 1; i++) {
            auto& c1 = polygon[i];
            auto& c2 = polygon[i + 1];
            if (is_intersect(c1, c2, polygon.front(), temp_point)) {
                return false;
            }
        }
        return true;
    }

    void confirm_temp() { push(temp_point.x, temp_point.y); }
    void finish() {
        if (polygon.size() > 2) {
            polygon.push_back(polygon.front());
        }
    }
    void undo_temp() {
        if (!polygon.empty()) {
            polygon.pop_back();
            recalculate();
        }
    }

    std::string name() const { return tag; }
    void set_name(std::string n) { tag = n; }
};

class Fl_Area : public map::Map, public Fl_Box {
   public:
    std::list<Area> areas;
    bool fill_areas = true;
    Area* temp;

    Fl_Area(int u, int v, size_t w, size_t h) : Fl_Box(u, v, w, h), Map(w, h, 1, 15) {
        temp = new Area(w, h, 255, 0, 0, 32);
        temp->set_name("SJTU");
        temp->push(0.83735, 0.409238);
        temp->push(0.837286, 0.409262);
        temp->push(0.837299, 0.409298);
        temp->push(0.837364, 0.409274);
    }

    ~Fl_Area() {
        delete temp;
    }

    void draw_area(Area* a, double x1, double y1, bool resize = true, bool has_temp = false, bool fill = true) {
        if (!a->visible()) {
            return;
        }
        if (a->is_clipped(lng, lat, x1, y1) && !has_temp) {
            if (a->points_count()) {
                auto [cx, cy] = cursor_mercator(Map::w / 2, Map::h / 2);
                a->indicator(cx, cy, Map::w, Map::h);
            }
            if (resize) {
                a->reset_anchor();
            }
            return;
        }
        /*if (DEBUG) {
            std::cout << "Area in screen" << std::endl;
        }*/
        if (fill_areas && fill) {
            a->fill(lng, lat, x1, y1, resize, has_temp);
        } else if (resize) {
            a->reset_anchor();
        }
        a->outline(lng, lat, pixels_per_side, has_temp);
    }

    void draw_areas(bool resize = true) {
        auto [x1, y1] = cursor_mercator(Map::w, Map::h);
        if (temp) {
            draw_area(temp, x1, y1, true, true);
            for (auto& a : areas) {
                draw_area(&a, x1, y1, resize, false, false);
            }
        } else {
            for (auto& a : areas) {
                draw_area(&a, x1, y1, resize);
            }
        }
    }

    void draw() { draw_areas(); }

    bool finish() {
        bool ret = false;
        if (temp->points_count() > 2 && temp->legal()) {
            temp->finish();
            areas.push_back(std::move(*temp));
            ret = true;
        }
        delete temp;
        temp = nullptr;
        return ret;
    }

    std::vector<std::pair<Fl_Color, std::string>> get_info() {
        std::vector<std::pair<Fl_Color, std::string>> ret;
        for (auto& a : areas) {
            ret.push_back(std::pair(a.color(), a.name()));
        }
        return ret;
    }
};
}  // namespace area

namespace map {

class Fl_Map : public Map, public Fl_Group {
    // Number of colums and rows of tilt images
    int cols, rows;
    // Images buffer for posision-only changes
    std::map<tilts::TiltId, Fl_Image*> redraw_buffer;
    std::list<tilts::TiltId> redraw_list;
    size_t max_cache_size;
    // Source of tilts and another buffer
    tilts::TiltsSource src;

    Fl_Offscreen oscr;
    int mouse_x = 0, mouse_y = 0;
    bool dragging = false;

   public:
    area::Fl_Area* areas;
    bool redraw_flag = 0, resize_flag = 0;

    // Offscreen drawing function for displaying map and areas
    void draw_map(bool resize = true) {
        redraw_flag = false;
        resize_flag = false;
#ifdef NO_MAP
        fl_rectf(0, 0, Map::w, Map::h, fl_rgb_color(252, 249, 242));
        areas->sync_with(*this);
        areas->draw_areas(resize);
        return;
#endif  // NO_MAP

        // Index for top left tilt
        auto tilt0 = mercator_to_tilt_id(lng, lat, z);

        // Screen coordinate for top left corner of top left tilt
        double xz = lng * tilts_per_side, yz = lat * tilts_per_side;
        int pixels_per_tilt = tilts::TILT_SIZE * k;
        int x0 = (int(xz) - xz) * pixels_per_tilt;
        int y0 = (int(yz) - yz) * pixels_per_tilt;

        // Displaying tilts
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                // Index for tilt[i, j]
                auto ti = tilt0.offset(i, j);
                // Index correction
                if (ti.x >= tilts_per_side) {
                    ti.x -= tilts_per_side;
                } else if (ti.x < 0) {
                    ti.x += tilts_per_side;
                }
                // Resized tilt image
                Fl_Image* pngResized;
                if (redraw_buffer.find(ti) != redraw_buffer.end()) {
                    // Cache hit
                    pngResized = redraw_buffer[ti];
                    if (DEBUG) {
                        // std::cout << "Cache Hit! " << ti << std::endl;
                    }
                } else {
                    // Get tilt from source
                    auto td = src.get(ti);
                    if (td.size == 0) {
                        // Not successfully requested yet
                        fl_rectf(x0 + i * pixels_per_tilt, y0 + j * pixels_per_tilt,
                                 pixels_per_tilt, pixels_per_tilt, fl_rgb_color(252, 249, 242));
                        continue;
                    }
                    auto png = Fl_PNG_Image(nullptr, td.buf, td.size);
                    pngResized = png.copy(pixels_per_tilt, pixels_per_tilt);
                    // Storing resized image into buffer
                    redraw_list.push_back(ti);
                    redraw_buffer[ti] = pngResized;
                }
                pngResized->draw(x0 + i * pixels_per_tilt, y0 + j * pixels_per_tilt);
                /*if (DEBUG) {
                    std::cout << "Putting tilt " << ti << " at ("
                        << x0 + i * pixels_per_tilt << ", "
                        << y0 + j * pixels_per_tilt << ")" << std::endl;
                }*/
            }
        }
        // Synchronising coordinate and zoom factors
        areas->sync_with(*this);
        areas->draw_areas(resize);
    }

    void draw_normal() {
        // Clear up buffer if exceeds the limit
        while (redraw_list.size() > max_cache_size) {
            delete redraw_buffer[redraw_list.front()];
            redraw_buffer.erase(redraw_list.front());
            redraw_list.pop_front();
        }

        fl_begin_offscreen(oscr);
        draw_map(false);
        fl_end_offscreen();
        fl_copy_offscreen(0, 0, Map::w, Map::h, oscr, 0, 0);
    }

    void draw_resize(bool disable_offscreen = false) {
        // Recreate buffer
        for (auto& p : redraw_buffer) {
            delete p.second;
        }
        redraw_buffer.clear();
        redraw_list.clear();

        if (DEBUG) {
            std::cout << "Resized! Cache Cleared." << std::endl;
        }

        if (disable_offscreen) {
            draw_map();
            return;
        }
        fl_begin_offscreen(oscr);
        draw_map();
        fl_end_offscreen();
        fl_copy_offscreen(0, 0, Map::w, Map::h, oscr, 0, 0);
    }

    void draw() override {
        if (resize_flag) {
            draw_resize();
        } else {
            draw_normal();
        }
    }

    void drag_screen_by(int dx, int dy) {
        translate(dx, dy);
        redraw_flag = true;

        if (DEBUG) {
            std::cout << "Dragged by dx = " << dx << ", dy = " << dy
                      << ", now lng = " << lng << ", lat = " << lat
                      << std::endl;
        }
    }

    void scroll_by(int dy, int mx, int my) {
        // Scaling speed limit, or the program will crash :(
        if (dy > 3) {
            dy = 3;
        } else if (dy < -3) {
            dy = -3;
        }
        auto z1 = z;

        if (scale(mx, my, powl(1.05, -dy))) {
            if (DEBUG) {
                std::cout << "Scrolled by dy = " << dy
                          << ", now z = " << z << ", k = " << k
                          << std::endl;
            }
            // draw_resize(z1 != z);
            redraw_flag = true;
            resize_flag = true;
        }
    }

    int handle(int event) override {
        switch (event) {
            case FL_ENTER: {
                return 1;
            }
            case FL_MOVE: {
                if (areas->temp) {
                    auto [x, y] = cursor_mercator(Fl::event_x(), Fl::event_y());
                    areas->temp->set_temp(x, y);
                    redraw_flag = true;
                }
                return 1;
            }
            case FL_LEAVE: {
                if (areas->temp) {
                    areas->temp->reset_temp();
                    redraw_flag = true;
                }
                return 1;
            }
            case FL_FOCUS: {
                return 1;
            }
            case FL_PUSH: {
                mouse_x = Fl::event_x();
                mouse_y = Fl::event_y();
                if (DEBUG) {
                    auto [x, y] = cursor_mercator(mouse_x, mouse_y);
                    std::cout << "Crusor x = " << x << ", y = " << y << std::endl;
                }
                return 1;
            }
            case FL_DRAG: {
                dragging = true;
                int dx = Fl::event_x() - mouse_x;
                int dy = Fl::event_y() - mouse_y;
                mouse_x = Fl::event_x();
                mouse_y = Fl::event_y();
                drag_screen_by(dx, dy);
                return 1;
            }
            case FL_RELEASE: {
                if (!dragging && areas->temp && areas->temp->legal()) {
                    areas->temp->confirm_temp();
                }
                dragging = false;
                return 1;
            }
            case FL_MOUSEWHEEL:
                scroll_by(Fl::event_dy(), Fl::event_x(), Fl::event_y());
        }
        return Fl_Widget::handle(event);
    }

    Fl_Map(int u, int v, size_t w, size_t h)
        : Fl_Group(u, v, w, h), Map(w, h, 1, 15), rows(int(w / tilts::TILT_SIZE) + 2), cols(int(h / tilts::TILT_SIZE) + 2), src(cols * rows * 30), max_cache_size(cols * rows * 5) {
        if (DEBUG) {
            std::cout << "Initializing map with rows = " << rows
                      << ", cols = " << cols << std::endl;
        }
        areas = new area::Fl_Area(0, 0, w, h);
        oscr = fl_create_offscreen(w, h);
    }

    ~Fl_Map() {
        for (auto& p : redraw_buffer) {
            delete p.second;
        }
        fl_delete_offscreen(oscr);
        delete areas;
    }

    std::tuple<bool, bool> poll_futures() { return src.pollFutures(); }
};
}  // namespace map

namespace control {
Fl_Double_Window* win;
map::Fl_Map* m;
area::Fl_Area* areas;

// Print the size of an area
class Fl_Area_Size_Output : public Fl_Widget {
   public:
    static const int lable_w = 6;
    Fl_Offscreen oscr;
    long double t_size = 0;
    bool legal = true;
    std::variant<area::Area*, std::list<area::Area>::iterator> t_area = nullptr;
    Fl_Area_Size_Output(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {
        oscr = fl_create_offscreen(w, h);
    }
    void draw() override {
        fl_begin_offscreen(oscr);
        fl_rectf(0, 0, w(), h(), FL_WHITE);
        fl_color(FL_BLACK);
        int t_h;
        switch (t_area.index()) {
            case 0: {
                auto a = std::get<0>(t_area);
                legal = a && a->size_legal();
                if (legal) {
                    t_size = a->temp_size();
                }
                t_h = h();
                fl_rectf(0, 0, lable_w, h(), a->color());
                break;
            }
            default: {
                legal = true;
                auto a = std::get<1>(t_area);
                t_size = a->size();
                fl_font(0, h() / 2.5);
                t_h = h() / 2;
                fl_rectf(0, 0, lable_w, h(), a->color());
                fl_draw(a->name().c_str(), 0, h() / 1.7, w(), h() / 2.5, FL_ALIGN_RIGHT);
                break;
            }
        }
        fl_font(0, t_h);
        fl_color(FL_BLACK);
        if (legal) {
            std::stringstream ss;
            if (t_size > 1e9) {
                t_size /= 1e6;
                ss << std::scientific << std::setprecision(3) << t_size << " km ";
            } else if (t_size > 1e4) {
                t_size /= 1e6;
                ss << std::fixed << std::setprecision(4) << t_size << " km ";
            } else {
                ss << std::setprecision(4) << t_size << " m ";
            }
            if (DEBUG) {
                std::cout << ss.str() << std::endl;
            }
            fl_draw(ss.str().c_str(), 0, 0, w(), t_h, FL_ALIGN_RIGHT);
            fl_font(1, t_h / 1.8);
            fl_draw("2", 0, 0, w(), t_h / 1.8, FL_ALIGN_RIGHT);
            // fl_draw((std::to_string(areas->temp->temp_size()) + " m^2").c_str(), x(), y(), w(), text_h, FL_ALIGN_RIGHT);
        } else {
            fl_draw("---", 0, 0, w(), t_h, FL_ALIGN_RIGHT);
        }
        fl_end_offscreen();
        fl_copy_offscreen(x(), y(), w(), h(), oscr, 0, 0);
    }
    ~Fl_Area_Size_Output() {
        fl_delete_offscreen(oscr);
    }
};

class Fl_Area_Info : public Fl_Group {
   public:
    static const int text_h = 60;
    Fl_Button *show_hide, *center_on;
    Fl_Area_Size_Output* size_op;
    std::list<area::Area>::iterator t_area;
    Fl_Area_Info(int w, int h) : Fl_Group(0, 0, w, h) {
        this->box(FL_DOWN_BOX);
        begin();
        color(FL_WHITE);
        auto p1 = new Fl_Pack(10, 10, w - 20, h - 20);
        p1->spacing(10);
        size_op = new Fl_Area_Size_Output(0, 0, w - 20, text_h);
        auto p2 = new Fl_Pack(0, 0, w, 30);
        p2->spacing(10);
        p2->type(FL_HORIZONTAL);
        show_hide = new Fl_Button(0, 0, w / 2 - 15, 30, "Hide");
        show_hide->callback(show_hide_cb, (void*)this);
        show_hide->color(FL_LIGHT3);
        center_on = new Fl_Button(0, 0, w / 2 - 15, 30, "Focus");
        center_on->callback(center_on_cb, (void*)this);
        center_on->color(FL_LIGHT3);
        p2->end();
        p1->end();
        end();
    }
    ~Fl_Area_Info() {
        delete show_hide;
        delete center_on;
        delete size_op;
    }
    void link(std::list<area::Area>::iterator a) {
        t_area = a;
        size_op->t_area = a;
    }

    static void show_hide_cb(Fl_Widget* o, void* v) {
        auto c = (Fl_Area_Info*)v;
        c->t_area->flip_visible();
        m->redraw_flag = true;
        ((Fl_Button*)o)->label((c->t_area->visible() ? "Hide" : "Show"));
        o->redraw();
        win->redraw();
    }
    static void center_on_cb(Fl_Widget* o, void* v) {
        auto c = (Fl_Area_Info*)v;
        auto cen = c->t_area->center();
        m->focus_on(cen.x, cen.y);
        m->redraw_flag = true;
        win->redraw();
    }
};

class Fl_Area_List : public Fl_Scroll {
   public:
    Fl_Pack* pack;
    Fl_Area_List(int x, int y, int w, int h) : Fl_Scroll(x, y, w, h) {
        this->type(Fl_Scroll::VERTICAL);
        this->color(FL_LIGHT3);
        this->scrollbar.color(FL_LIGHT3);
        this->scrollbar.color2(FL_LIGHT3);
        begin();
        int nbuttons = 24;
        pack = new Fl_Pack(x, y, 240, 285);
        pack->end();
        pack->spacing(10);
        pack->color(FL_WHITE);
        end();
    }
    ~Fl_Area_List() { delete pack; }
}* area_list;

class Fl_New_Area_Control : public Fl_Group {
   public:
    const int text_h = 30;
    size_t count = 0;
    Fl_Button *confirm_new, *undo, *new_area;
    Fl_Input* area_name;
    Fl_Area_Size_Output* size_op;
    Fl_New_Area_Control(int x, int y, int w, int h) : Fl_Group(x, y, w, h) {
        this->box(FL_DOWN_BOX);
        begin();
        color(FL_WHITE);
        confirm_new = new Fl_Button(x + 10, y + 150, w - 20, 30, "Confirm");
        confirm_new->callback(confirm_cb, (void*)this);
        undo = new Fl_Button(x + 10, y + 115, w - 20, 30, "Undo");
        undo->callback(undo_cb, (void*)this);
        area_name = new Fl_Input(x + 10, y + 60, w - 20, 30);
        area_name->value("SJTU");
        area_name->textsize(20);
        area_name->callback(rename_cb, (void*)this);
        area_name->when(FL_WHEN_CHANGED);
        size_op = new Fl_Area_Size_Output(x + 10, y + 15, w - 20, text_h);
        end();
        new_area = new Fl_Button(x + 10, y + 150, w - 20, 30, "New");
        new_area->callback(new_area_cb, (void*)this);
        new_area->hide();
    }

    void link() { size_op->t_area = areas->temp; }
    void update_size() { size_op->draw(); }
    void update_name() {
        std::string str = area_name->value();
        if (str.empty()) {
            str = "Area " + std::to_string(count);
            // area_name->value(str.c_str());
            // area_name->redraw();
        }
        if (DEBUG) {
            std::cout << str << std::endl;
        }
        areas->temp->set_name(str.c_str());
        win->redraw();
        update_size();
    }

    static void confirm_cb(Fl_Widget* o, void* v) {
        assert(areas->temp);
        if (areas->finish()) {
            auto info = new Fl_Area_Info(240, 120);
            area_list->pack->add(info);
            info->link(--(areas->areas.end()));
            area_list->redraw();
        }
        m->redraw_flag = 1;
        auto c = (Fl_New_Area_Control*)v;
        c->new_area->show();
        c->hide();
        c->new_area->take_focus();
    }
    static void new_area_cb(Fl_Widget* o, void* v) {
        auto [r, g, b] = area::color_generator();
        assert(!areas->temp);
        auto c = (Fl_New_Area_Control*)v;
        ++(c->count);
        areas->temp = new area::Area(m->Fl_Group::w(), m->Fl_Group::h(), r, g, b, 32);
        std::string str = "Area " + std::to_string(c->count);
        c->area_name->value(str.c_str());
        areas->temp->set_name(str);
        c->size_op->t_area = areas->temp;
        m->redraw_flag = true;
        c->show();
        c->redraw();
        c->update_size();
        c->take_focus();
        ((Fl_Button*)o)->hide();
    }
    static void undo_cb(Fl_Widget* o, void* v) {
        if (areas->temp) {
            areas->temp->undo_temp();
        }
        m->redraw_flag = true;
        win->redraw();
        ((Fl_New_Area_Control*)v)->update_size();
    }
    static void rename_cb(Fl_Widget* o, void* v) {
        auto c = (Fl_New_Area_Control*)v;
        c->update_name();
    }
}* new_area_control;
}  // namespace control

bool poll_future_flag = false;
void poll_future_handler(void*) {
    poll_future_flag = false;
    if (DEBUG) {
        std::cout << "Timeout retriggered" << std::endl;
    }
    // Fl::repeat_timeout(0.5, poll_future_handler);
    // control::m->redraw_flag = true;
}

int main() {
    Fl::visual(FL_DOUBLE | FL_RGB);
    fl_register_images();
    Fl::scheme("gtk+");

    control::win = new Fl_Double_Window(1280, 800, "Map");
    control::win->color(FL_LIGHT3);
    control::area_list = new control::Fl_Area_List(1020, 20, 255, 700);
    control::new_area_control = new control::Fl_New_Area_Control(1020, 590, 240, 190);
    control::m = new map::Fl_Map(0, 0, 1000, 800);
    control::areas = control::m->areas;
    control::new_area_control->link();
    control::new_area_control->take_focus();
    control::win->end();
    control::win->show();
    Fl::add_timeout(0.5, poll_future_handler);

    while (true) {
        auto [poll, update] = control::m->poll_futures();
        control::m->redraw_flag |= update;
        if (control::m->redraw_flag) {
            control::win->redraw();
            // control::m->draw();
            if (control::areas->temp) {
                control::new_area_control->update_size();
            }
        }
        if (poll) {
            if (!poll_future_flag) {
                poll_future_flag = true;
                Fl::add_timeout(0.2, poll_future_handler);
            }
        }
        auto nWin = Fl::wait();
        if (nWin == 0) {
            break;
        }
    }
    return 0;
}
