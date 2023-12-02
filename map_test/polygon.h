#pragma once
//
//  polygon.h
// 
//  Tools to determine the relationship of points, rays and line segments
//

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
            ((q2.x - p1.x) * (p2.y - p1.y) - (p2.x - p1.x) * (q2.y - p1.y)) >= 0 ||
            ((p1.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p1.y - q1.y)) *
            ((p2.x - q1.x) * (q2.y - q1.y) - (q2.x - q1.x) * (p2.y - q1.y)) >= 0) {
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
        return { hsl_val(cmin,cmax,h + 120) * 255 ,hsl_val(cmin,cmax,h) * 255 ,hsl_val(cmin,cmax,h - 120) * 255 };
    }

    std::tuple<uchar, uchar, uchar> color_generator() {
        static std::default_random_engine e;
        static int hue = 0;
        std::uniform_real_distribution<double> h(180, 300), l(0.45, 0.75), s(0.75, 1);
        hue = int(hue + h(e)) % 360;
        return hsl_to_rgb(hue, s(e), l(e));
    }
}