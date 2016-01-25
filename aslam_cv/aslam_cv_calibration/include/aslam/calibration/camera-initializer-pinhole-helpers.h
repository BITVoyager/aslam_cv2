#ifndef ASLAM_CALIBRATION_CAMERA_INITIALIZER_PINHOLE_HELPERS_H
#define ASLAM_CALIBRATION_CAMERA_INITIALIZER_PINHOLE_HELPERS_H

class PinholeHelpers {
 public:

  template<typename scalar>
  static inline scalar square(scalar x) {
    return x*x;
  }

  static inline double hypot(double a, double b) {
    return sqrt(square(a) + square(b));
  }

  static std::vector<cv::Point2d> * intersectCircles(double x1, double y1, double r1,
                                            double x2, double y2, double r2) {
    std::vector<cv::Point2d> ipts;

    double d = hypot(x1 - x2, y1 - y2);
    if (d > r1 + r2) {
      // Circles do not intersect.
      return ipts;
    }

    if (d < fabs(r1 - r2)) {
      // One circle is contained within the other.
      return ipts;
    }

    double a = (square(r1) - square(r2) + square(d)) / (2.0 * d);
    double h = sqrt(square(r1) - square(a));

    double x3 = x1 + a * (x2 - x1) / d;
    double y3 = y1 + a * (y2 - y1) / d;

    if (h < 1e-10) {
      // Two circles touch at one point.
      ipts.emplace_back(x3, y3);
      return ipts;
    }

    ipts.emplace_back(x3 + h * (y2 - y1) / d, y3 - h * (x2 - x1) / d);
    ipts.emplace_back(x3 - h * (y2 - y1) / d, y3 + h * (x2 - x1) / d);
    return &ipts;
  }

  static void fitCircle(const std::vector<cv::Point2d>& points, double& center_x,
                        double& center_y, double& radius) {
    // D. Umbach, and K. Jones, A Few Methods for Fitting Circles to Data,
    // IEEE Transactions on Instrumentation and Measurement, 2000
    // We use the modified least squares method.
    double sum_x = 0.0;
    double sum_y = 0.0;
    double sum_xx = 0.0;
    double sum_xy = 0.0;
    double sum_yy = 0.0;
    double sum_xxx = 0.0;
    double sum_xxy = 0.0;
    double sum_xyy = 0.0;
    double sum_yyy = 0.0;

    int n = points.size();
    for (int i = 0; i < n; ++i) {
      double x = points.at(i).x;
      double y = points.at(i).y;

      sum_x += x;
      sum_y += y;
      sum_xx += x * x;
      sum_xy += x * y;
      sum_yy += y * y;
      sum_xxx += x * x * x;
      sum_xxy += x * x * y;
      sum_xyy += x * y * y;
      sum_yyy += y * y * y;
    }

    double A = n * sum_xx - square(sum_x);
    double B = n * sum_xy - sum_x * sum_y;
    double C = n * sum_yy - square(sum_y);
    double D = 0.5 *
        (n * sum_xyy - sum_x * sum_yy + n * sum_xxx - sum_x * sum_xx);
    double E = 0.5 *
        (n * sum_xxy - sum_y * sum_xx + n * sum_yyy - sum_y * sum_yy);

    center_x = (D * C - B * E) / (A * C - square(B));
    center_y = (A * E - B * D) / (A * C - square(B));

    double sum_r = 0.0;
    for (int i = 0; i < n; ++i) {
      double x = points.at(i).x;
      double y = points.at(i).y;
      sum_r += hypot(x - center_x, y - center_y);
    }
    radius = sum_r / n;
  }

}; // class PinholeHelpers

#endif  // ASLAM_CALIBRATION_CAMERA_INITIALIZER_PINHOLE_HELPERS_H
