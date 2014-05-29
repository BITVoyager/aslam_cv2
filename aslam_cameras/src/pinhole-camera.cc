#include <opencv2/core/eigen.hpp>

namespace aslam {

namespace cameras {

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T>::PinholeProjection()
    : _fu(0.0),
      _fv(0.0),
      _cu(0.0),
      _cv(0.0),
      _ru(0),
      _rv(0) {
  updateTemporaries();
}

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T>::PinholeProjection(
    const sm::PropertyTree & config)
    : _distortion(sm::PropertyTree(config, "distortion")) {
  _fu = config.getDouble("fu");
  _fv = config.getDouble("fv");
  _cu = config.getDouble("cu");
  _cv = config.getDouble("cv");
  _ru = config.getInt("ru");
  _rv = config.getInt("rv");

  updateTemporaries();
}

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T>::PinholeProjection(double focalLengthU,
                                                   double focalLengthV,
                                                   double imageCenterU,
                                                   double imageCenterV,
                                                   int resolutionU,
                                                   int resolutionV,
                                                   distortion_t distortion)
    : _fu(focalLengthU),
      _fv(focalLengthV),
      _cu(imageCenterU),
      _cv(imageCenterV),
      _ru(resolutionU),
      _rv(resolutionV),
      _distortion(distortion) {
  updateTemporaries();
}

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T>::PinholeProjection(double focalLengthU,
                                                   double focalLengthV,
                                                   double imageCenterU,
                                                   double imageCenterV,
                                                   int resolutionU,
                                                   int resolutionV)
    : _fu(focalLengthU),
      _fv(focalLengthV),
      _cu(imageCenterU),
      _cv(imageCenterV),
      _ru(resolutionU),
      _rv(resolutionV) {
  updateTemporaries();
}

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T>::~PinholeProjection() {
}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_K>
bool PinholeProjection<DISTORTION_T>::euclideanToKeypoint(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_K> & outKeypointConst) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);
  Eigen::MatrixBase<DERIVED_K> & outKeypoint = const_cast<Eigen::MatrixBase<
      DERIVED_K> &>(outKeypointConst);

  outKeypoint.derived().resize(2);
  double rz = 1.0 / p[2];
  outKeypoint[0] = p[0] * rz;
  outKeypoint[1] = p[1] * rz;

  _distortion.distort(outKeypoint);

  outKeypoint[0] = _fu * outKeypoint[0] + _cu;
  outKeypoint[1] = _fv * outKeypoint[1] + _cv;

  return isValid(outKeypoint) && p[2] > 0;

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_K, typename DERIVED_JP>
bool PinholeProjection<DISTORTION_T>::euclideanToKeypoint(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_K> & outKeypointConst,
    const Eigen::MatrixBase<DERIVED_JP> & outJp) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JP>, 2, 3);

  Eigen::MatrixBase<DERIVED_K> & outKeypoint = const_cast<Eigen::MatrixBase<
      DERIVED_K> &>(outKeypointConst);
  outKeypoint.derived().resize(2);

  // Jacobian:
  Eigen::MatrixBase<DERIVED_JP> & J =
      const_cast<Eigen::MatrixBase<DERIVED_JP> &>(outJp);
  J.derived().resize(KeypointDimension, 3);
  J.setZero();

  double rz = 1.0 / p[2];
  double rz2 = rz * rz;

  outKeypoint[0] = p[0] * rz;
  outKeypoint[1] = p[1] * rz;

  Eigen::MatrixXd Jd;
  _distortion.distort(outKeypoint, Jd);  // distort and Jacobian wrt. keypoint

  // Jacobian including distortion
  J(0, 0) = _fu * Jd(0, 0) * rz;
  J(0, 1) = _fu * Jd(0, 1) * rz;
  J(0, 2) = -_fu * (p[0] * Jd(0, 0) + p[1] * Jd(0, 1)) * rz2;
  J(1, 0) = _fv * Jd(1, 0) * rz;
  J(1, 1) = _fv * Jd(1, 1) * rz;
  J(1, 2) = -_fv * (p[0] * Jd(1, 0) + p[1] * Jd(1, 1)) * rz2;

  outKeypoint[0] = _fu * outKeypoint[0] + _cu;
  outKeypoint[1] = _fv * outKeypoint[1] + _cv;

  return isValid(outKeypoint) && p[2] > 0;

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_K>
bool PinholeProjection<DISTORTION_T>::homogeneousToKeypoint(
    const Eigen::MatrixBase<DERIVED_P> & ph,
    const Eigen::MatrixBase<DERIVED_K> & outKeypoint) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);

  // hope this works... (required to have valid static asserts)
  if (ph[3] < 0)
    return euclideanToKeypoint(-ph.derived().template head<3>(), outKeypoint);
  else
    return euclideanToKeypoint(ph.derived().template head<3>(), outKeypoint);
}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_K, typename DERIVED_JP>
bool PinholeProjection<DISTORTION_T>::homogeneousToKeypoint(
    const Eigen::MatrixBase<DERIVED_P> & ph,
    const Eigen::MatrixBase<DERIVED_K> & outKeypoint,
    const Eigen::MatrixBase<DERIVED_JP> & outJp) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JP>, 2, 4);

  Eigen::MatrixBase<DERIVED_JP> & J =
      const_cast<Eigen::MatrixBase<DERIVED_JP> &>(outJp);
  J.derived().resize(KeypointDimension, 4);
  J.setZero();

  // hope this works... (required to have valid static asserts)
  return euclideanToKeypoint(ph.derived().template head<3>(), outKeypoint,
                             J.derived().template topLeftCorner<2, 3>());

  if (ph[3] < 0) {
    bool success = euclideanToKeypoint(
        -ph.derived().template head<3>(), outKeypoint,
        J.derived().template topLeftCorner<2, 3>());
    J = -J;
    return success;
  } else {
    return euclideanToKeypoint(ph.derived().template head<3>(), outKeypoint,
                               J.derived().template topLeftCorner<2, 3>());
  }
}

template<typename DISTORTION_T>
template<typename DERIVED_K, typename DERIVED_P>
bool PinholeProjection<DISTORTION_T>::keypointToEuclidean(
    const Eigen::MatrixBase<DERIVED_K> & keypoint,
    const Eigen::MatrixBase<DERIVED_P> & outPointConst) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);

  keypoint_t kp = keypoint;

  kp[0] = (kp[0] - _cu) / _fu;
  kp[1] = (kp[1] - _cv) / _fv;
  _distortion.undistort(kp);  // revert distortion

  Eigen::MatrixBase<DERIVED_P> & outPoint = const_cast<Eigen::MatrixBase<
      DERIVED_P> &>(outPointConst);
  outPoint.derived().resize(3);

  outPoint[0] = kp[0];
  outPoint[1] = kp[1];
  outPoint[2] = 1;

  return isValid(keypoint);

}

template<typename DISTORTION_T>
template<typename DERIVED_K, typename DERIVED_P, typename DERIVED_JK>
bool PinholeProjection<DISTORTION_T>::keypointToEuclidean(
    const Eigen::MatrixBase<DERIVED_K> & keypoint,
    const Eigen::MatrixBase<DERIVED_P> & outPointConst,
    const Eigen::MatrixBase<DERIVED_JK> & outJk) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JK>, 3, 2);

  keypoint_t kp = keypoint;

  kp[0] = (kp[0] - _cu) / _fu;
  kp[1] = (kp[1] - _cv) / _fv;

  Eigen::MatrixXd Jd(2, 2);

  _distortion.undistort(kp, Jd);  // revert distortion

  Eigen::MatrixBase<DERIVED_P> & outPoint = const_cast<Eigen::MatrixBase<
      DERIVED_P> &>(outPointConst);
  outPoint.derived().resize(3);

  outPoint[0] = kp[0];
  outPoint[1] = kp[1];
  outPoint[2] = 1;

  Eigen::MatrixBase<DERIVED_JK> & J =
      const_cast<Eigen::MatrixBase<DERIVED_JK> &>(outJk);
  J.derived().resize(3, KeypointDimension);
  J.setZero();

  //J.derived()(0,0) = 1.0;
  //J.derived()(1,1) = _fu_over_fv;

  J.derived()(0, 0) = _recip_fu;
  J.derived()(1, 1) = _recip_fv;

  J *= Jd;

  return isValid(keypoint);

}

template<typename DISTORTION_T>
template<typename DERIVED_K, typename DERIVED_P>
bool PinholeProjection<DISTORTION_T>::keypointToHomogeneous(
    const Eigen::MatrixBase<DERIVED_K> & keypoint,
    const Eigen::MatrixBase<DERIVED_P> & outPoint) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);

  Eigen::MatrixBase<DERIVED_P> & p =
      const_cast<Eigen::MatrixBase<DERIVED_P> &>(outPoint);
  p.derived().resize(4);
  p[3] = 0.0;
  return keypointToEuclidean(keypoint, p.derived().template head<3>());

}

template<typename DISTORTION_T>
template<typename DERIVED_K, typename DERIVED_P, typename DERIVED_JK>
bool PinholeProjection<DISTORTION_T>::keypointToHomogeneous(
    const Eigen::MatrixBase<DERIVED_K> & keypoint,
    const Eigen::MatrixBase<DERIVED_P> & outPoint,
    const Eigen::MatrixBase<DERIVED_JK> & outJk) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_K>, 2);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JK>, 4, 2);

  Eigen::MatrixBase<DERIVED_JK> & Jk =
      const_cast<Eigen::MatrixBase<DERIVED_JK> &>(outJk);
  Jk.derived().resize(2, 4);
  Jk.setZero();

  Eigen::MatrixBase<DERIVED_P> & p =
      const_cast<Eigen::MatrixBase<DERIVED_P> &>(outPoint);
  p[3] = 0.0;

  return keypointToEuclidean(keypoint, p.template head<3>(),
                             Jk.template topLeftCorner<3, 2>());

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_JI>
void PinholeProjection<DISTORTION_T>::euclideanToKeypointIntrinsicsJacobian(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_JI> & outJi) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JI>, 2, 4);

  Eigen::MatrixBase<DERIVED_JI> & J =
      const_cast<Eigen::MatrixBase<DERIVED_JI> &>(outJi);
  J.derived().resize(KeypointDimension, 4);
  J.setZero();

  double rz = 1.0 / p[2];

  keypoint_t kp;
  kp[0] = p[0] * rz;
  kp[1] = p[1] * rz;
  _distortion.distort(kp);

  J(0, 0) = kp[0];
  J(0, 2) = 1;

  J(1, 1) = kp[1];
  J(1, 3) = 1;

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_JD>
void PinholeProjection<DISTORTION_T>::euclideanToKeypointDistortionJacobian(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_JD> & outJd) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 3);

  double rz = 1.0 / p[2];
  keypoint_t kp;
  kp[0] = p[0] * rz;
  kp[1] = p[1] * rz;

  _distortion.distortParameterJacobian(kp, outJd);

  Eigen::MatrixBase<DERIVED_JD> & J =
      const_cast<Eigen::MatrixBase<DERIVED_JD> &>(outJd);
  J.derived().resize(KeypointDimension, _distortion.minimalDimensions());

  J.row(0) *= _fu;
  J.row(1) *= _fv;

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_JI>
void PinholeProjection<DISTORTION_T>::homogeneousToKeypointIntrinsicsJacobian(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_JI> & outJi) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_JI>, 2, 4);

  if (p[3] < 0.0) {
    euclideanToKeypointIntrinsicsJacobian(-p.derived().template head<3>(),
                                          outJi);
  } else {
    euclideanToKeypointIntrinsicsJacobian(p.derived().template head<3>(),
                                          outJi);
  }

}

template<typename DISTORTION_T>
template<typename DERIVED_P, typename DERIVED_JD>
void PinholeProjection<DISTORTION_T>::homogeneousToKeypointDistortionJacobian(
    const Eigen::MatrixBase<DERIVED_P> & p,
    const Eigen::MatrixBase<DERIVED_JD> & outJd) const {

  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE_OR_DYNAMIC(
      Eigen::MatrixBase<DERIVED_P>, 4);

  if (p[3] < 0.0) {
    euclideanToKeypointDistortionJacobian(-p.derived().template head<3>(),
                                          outJd);
  } else {
    euclideanToKeypointDistortionJacobian(p.derived().template head<3>(),
                                          outJd);
  }

}

template<typename DISTORTION_T>
template<class Archive>
void PinholeProjection<DISTORTION_T>::save(Archive & ar,
                                           const unsigned int /* version */) const {
  ar << BOOST_SERIALIZATION_NVP(_fu);
  ar << BOOST_SERIALIZATION_NVP(_fv);
  ar << BOOST_SERIALIZATION_NVP(_cu);
  ar << BOOST_SERIALIZATION_NVP(_cv);
  ar << BOOST_SERIALIZATION_NVP(_ru);
  ar << BOOST_SERIALIZATION_NVP(_rv);
  ar << BOOST_SERIALIZATION_NVP(_distortion);

}

template<typename DISTORTION_T>
template<class Archive>
void PinholeProjection<DISTORTION_T>::load(Archive & ar,
                                           const unsigned int version) {
  SM_ASSERT_LE(std::runtime_error, version,
               (unsigned int) CLASS_SERIALIZATION_VERSION,
               "Unsupported serialization version");

  ar >> BOOST_SERIALIZATION_NVP(_fu);
  ar >> BOOST_SERIALIZATION_NVP(_fv);
  ar >> BOOST_SERIALIZATION_NVP(_cu);
  ar >> BOOST_SERIALIZATION_NVP(_cv);
  ar >> BOOST_SERIALIZATION_NVP(_ru);
  ar >> BOOST_SERIALIZATION_NVP(_rv);
  ar >> BOOST_SERIALIZATION_NVP(_distortion);

  updateTemporaries();
}

// \brief creates a random valid keypoint.
template<typename DISTORTION_T>
Eigen::VectorXd PinholeProjection<DISTORTION_T>::createRandomKeypoint() const {
  Eigen::VectorXd out(2, 1);
  out.setRandom();
  out(0) = fabs(out(0)) * _ru;
  out(1) = fabs(out(1)) * _rv;
  return out;
}

// \brief creates a random visible point. Negative depth means random between 0 and 100 meters.
template<typename DISTORTION_T>
Eigen::Vector3d PinholeProjection<DISTORTION_T>::createRandomVisiblePoint(
    double depth) const {
  Eigen::VectorXd y = createRandomKeypoint();
  Eigen::Vector3d p;
  keypointToEuclidean(y, p);

  if (depth < 0.0) {
    depth = ((double) rand() / (double) RAND_MAX) * 100.0;
  }

  p /= p.norm();

  // Muck with the depth. This doesn't change the pointing direction.
  p *= depth;
  return p;

}

template<typename DISTORTION_T>
template<typename DERIVED_K>
bool PinholeProjection<DISTORTION_T>::isValid(
    const Eigen::MatrixBase<DERIVED_K> & keypoint) const {
  return keypoint[0] >= 0 && keypoint[1] >= 0 && keypoint[0] < (double) _ru
      && keypoint[1] < (double) _rv;

}

template<typename DISTORTION_T>
template<typename DERIVED_P>
bool PinholeProjection<DISTORTION_T>::isEuclideanVisible(
    const Eigen::MatrixBase<DERIVED_P> & p) const {
  keypoint_t k;
  return euclideanToKeypoint(p, k);

}

template<typename DISTORTION_T>
template<typename DERIVED_P>
bool PinholeProjection<DISTORTION_T>::isHomogeneousVisible(
    const Eigen::MatrixBase<DERIVED_P> & ph) const {
  keypoint_t k;
  return homogeneousToKeypoint(ph, k);
}

template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::update(const double * v) {
  _fu += v[0];
  _fv += v[1];
  _cu += v[2];
  _cv += v[3];
  _recip_fu = 1.0 / _fu;
  _recip_fv = 1.0 / _fv;
  _fu_over_fv = _fu / _fv;
}

template<typename DISTORTION_T>
int PinholeProjection<DISTORTION_T>::minimalDimensions() const {
  return IntrinsicsDimension;
}

template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::getParameters(Eigen::MatrixXd & P) const {
  P.resize(4, 1);
  P(0, 0) = _fu;
  P(1, 0) = _fv;
  P(2, 0) = _cu;
  P(3, 0) = _cv;
}

template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::setParameters(const Eigen::MatrixXd & P) {
  _fu = P(0, 0);
  _fv = P(1, 0);
  _cu = P(2, 0);
  _cv = P(3, 0);
  updateTemporaries();
}

template<typename DISTORTION_T>
Eigen::Vector2i PinholeProjection<DISTORTION_T>::parameterSize() const {
  return Eigen::Vector2i(4, 1);
}

template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::updateTemporaries() {
  _recip_fu = 1.0 / _fu;
  _recip_fv = 1.0 / _fv;
  _fu_over_fv = _fu / _fv;

}

template<typename DISTORTION_T>
bool PinholeProjection<DISTORTION_T>::isBinaryEqual(
    const PinholeProjection<DISTORTION_T> & rhs) const {
  return _fu == rhs._fu && _fv == rhs._fv && _cu == rhs._cu && _cv == rhs._cv
      && _ru == rhs._ru && _rv == rhs._rv && _recip_fu == rhs._recip_fu
      && _recip_fv == rhs._recip_fv && _fu_over_fv == rhs._fu_over_fv
      && _distortion.isBinaryEqual(rhs._distortion);
}

template<typename DISTORTION_T>
PinholeProjection<DISTORTION_T> PinholeProjection<DISTORTION_T>::getTestProjection() {
  return PinholeProjection<DISTORTION_T>(400, 400, 320, 240, 640, 480,
                                         DISTORTION_T::getTestDistortion());
}

template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::resizeIntrinsics(double scale) {
  _fu *= scale;
  _fv *= scale;
  _cu *= scale;
  _cv *= scale;
  _ru = _ru * scale;
  _rv = _rv * scale;

  updateTemporaries();
}

/// \brief Get a set of border rays
template<typename DISTORTION_T>
void PinholeProjection<DISTORTION_T>::getBorderRays(Eigen::MatrixXd & rays) {
  rays.resize(4, 8);
  keypointToHomogeneous(Eigen::Vector2d(0.0, 0.0), rays.col(0));
  keypointToHomogeneous(Eigen::Vector2d(0.0, _rv * 0.5), rays.col(1));
  keypointToHomogeneous(Eigen::Vector2d(0.0, _rv - 1.0), rays.col(2));
  keypointToHomogeneous(Eigen::Vector2d(_ru - 1.0, 0.0), rays.col(3));
  keypointToHomogeneous(Eigen::Vector2d(_ru - 1.0, _rv * 0.5), rays.col(4));
  keypointToHomogeneous(Eigen::Vector2d(_ru - 1.0, _rv - 1.0), rays.col(5));
  keypointToHomogeneous(Eigen::Vector2d(_ru * 0.5, 0.0), rays.col(6));
  keypointToHomogeneous(Eigen::Vector2d(_ru * 0.5, _rv - 1.0), rays.col(7));

}

namespace detailPinhole {

inline double square(double x) {
  return x * x;
}
inline float square(float x) {
  return x * x;
}
inline double hypot(double a, double b) {
  return sqrt(square(a) + square(b));
}

}  // namespace detail

/// \brief initialize the intrinsics based on one view of a gridded calibration target
/// \return true on success
///
/// These functions were developed with the help of Lionel Heng and the excellent camodocal
/// https://github.com/hengli/camodocal
template<typename DISTORTION_T>
bool PinholeProjection<DISTORTION_T>::initializeIntrinsics(const std::vector<GridCalibrationTargetObservation> &observations) {
  SM_DEFINE_EXCEPTION(Exception, std::runtime_error);
  SM_ASSERT_TRUE(Exception, observations.size() != 0, "Need min. one observation");

  if(observations.size()>1)
    SM_DEBUG_STREAM("pinhole camera model only supports one observation for intrinsics initialization! (using first image)");

  GridCalibrationTargetObservation obs = observations[0];

  using detailPinhole::square;
  using detailPinhole::hypot;
  if (!obs.target()) {
    SM_ERROR("The GridCalibrationTargetObservation has no target object");
    return false;
  }

  // First, initialize the image center at the center of the image.
  _cu = (obs.imCols()-1.0) / 2.0;
  _cv = (obs.imRows()-1.0) / 2.0;
  _ru = obs.imCols();
  _rv = obs.imRows();

  _distortion.clear();

  // Grab a reference to the target for easy access.
  const GridCalibrationTargetBase & target = *obs.target();

  /// Initialize some temporaries needed.
  double gamma0 = 0.0;
  double minReprojErr = std::numeric_limits<double>::max();

  // Now we try to find a non-radial line to initialize the focal length
  bool success = false;
  for (size_t r = 0; r < target.rows(); ++r) {
    // Grab all the valid corner points for this checkerboard observation
    cv::Mat P(target.cols(), 4, CV_64F);
    size_t count = 0;
    for (size_t c = 0; c < target.cols(); ++c) {
      Eigen::Vector2d imagePoint;
      Eigen::Vector3d gridPoint;
      if (obs.imageGridPoint(r, c, imagePoint)) {
        double u = imagePoint[0] - _cu;
        double v = imagePoint[1] - _cv;
        P.at<double>(count, 0) = u;
        P.at<double>(count, 1) = v;
        P.at<double>(count, 2) = 0.5;
        P.at<double>(count, 3) = -0.5 * (square(u) + square(v));
        ++count;
      }
    }

    const size_t MIN_CORNERS = 3;
    // MIN_CORNERS is an arbitrary threshold for the number of corners
    if (count > MIN_CORNERS) {
      // Resize P to fit with the count of valid points.
      cv::Mat C;
      cv::SVD::solveZ(P.rowRange(0, count), C);

      double t = square(C.at<double>(0)) + square(C.at<double>(1))
          + C.at<double>(2) * C.at<double>(3);
      if (t < 0) {
        SM_DEBUG_STREAM("Skipping a bad SVD solution on row " << r);
        continue;
      }

      // check that line image is not radial
      double d = sqrt(1.0 / t);
      double nx = C.at<double>(0) * d;
      double ny = C.at<double>(1) * d;
      if (hypot(nx, ny) > 0.95) {
        SM_DEBUG_STREAM("Skipping a radial line on row " << r);
        continue;
      }

      double nz = sqrt(1.0 - square(nx) - square(ny));
      double gamma = fabs(C.at<double>(2) * d / nz);

      SM_DEBUG_STREAM("Testing a focal length estimate of " << gamma);
      _fu = gamma;
      _fv = gamma;
      updateTemporaries();
      sm::kinematics::Transformation T_target_camera;
      if (!estimateTransformation(obs, T_target_camera)) {
        SM_DEBUG_STREAM(
            "Skipping row " << r
                << " as the transformation estimation failed.");
        continue;
      }

      double reprojErr = 0.0;
      size_t numReprojected = computeReprojectionError(obs, T_target_camera,
                                                       reprojErr);

      if (numReprojected > MIN_CORNERS) {
        double avgReprojErr = reprojErr / numReprojected;

        if (avgReprojErr < minReprojErr) {
          SM_DEBUG_STREAM(
              "Row " << r << " produced the new best estimate: " << avgReprojErr
                  << " < " << minReprojErr);
          minReprojErr = avgReprojErr;
          gamma0 = gamma;
          success = true;
        }
      }

    }  // If this observation has enough valid corners
    else {
      SM_DEBUG_STREAM(
          "Skipping row " << r << " because it only had " << count
              << " corners. Minimum: " << MIN_CORNERS);
    }
  }  // For each row in the image.

  _fu = gamma0;
  _fv = gamma0;
  updateTemporaries();
  return success;
}

template<typename DISTORTION_T>
size_t PinholeProjection<DISTORTION_T>::computeReprojectionError(
    const GridCalibrationTargetObservation & obs,
    const sm::kinematics::Transformation & T_target_camera,
    double & outErr) const {
  outErr = 0.0;
  size_t count = 0;
  sm::kinematics::Transformation T_camera_target = T_target_camera.inverse();

  for (size_t i = 0; i < obs.target()->size(); ++i) {
    Eigen::Vector2d y, yhat;
    if (obs.imagePoint(i, y)
        && euclideanToKeypoint(T_camera_target * obs.target()->point(i),
                               yhat)) {
      outErr += (y - yhat).norm();
      ++count;
    }
  }

  return count;
}

/// \brief estimate the transformation of the camera with respect to the calibration target
///        On success out_T_t_c is filled in with the transformation that takes points from
///        the camera frame to the target frame
/// \return true on success
template<typename DISTORTION_T>
bool PinholeProjection<DISTORTION_T>::estimateTransformation(
    const GridCalibrationTargetObservation & obs,
    sm::kinematics::Transformation & out_T_t_c) const {

  std::vector<cv::Point2f> Ms;
  std::vector<cv::Point3f> Ps;

  // Get the observed corners in the image and target frame
  obs.getCornersImageFrame(Ms);
  obs.getCornersTargetFrame(Ps);

  // Convert all target corners to a fakey pinhole view.
  size_t count = 0;
  for (size_t i = 0; i < Ms.size(); ++i) {
    Eigen::Vector3d targetPoint(Ps[i].x, Ps[i].y, Ps[i].z);
    Eigen::Vector2d imagePoint(Ms[i].x, Ms[i].y);
    Eigen::Vector3d backProjection;

    if (keypointToEuclidean(imagePoint, backProjection)
        && backProjection[2] > 0.0) {
      double x = backProjection[0];
      double y = backProjection[1];
      double z = backProjection[2];
      Ps.at(count).x = targetPoint[0];
      Ps.at(count).y = targetPoint[1];
      Ps.at(count).z = targetPoint[2];

      Ms.at(count).x = x / z;
      Ms.at(count).y = y / z;
      ++count;
    } else {
      SM_DEBUG_STREAM(
          "Skipping point " << i << ", point was observed: " << imagePoint
              << ", projection success: "
              << keypointToEuclidean(imagePoint, backProjection)
              << ", in front of camera: " << (backProjection[2] > 0.0)
              << "image point: " << imagePoint.transpose()
              << ", backProjection: " << backProjection.transpose()
              << ", camera params (fu,fv,cu,cv):" << fu() << ", " << fv()
              << ", " << cu() << ", " << cv());
    }
  }

  Ps.resize(count);
  Ms.resize(count);

  std::vector<double> distCoeffs(4, 0.0);

  cv::Mat rvec(3, 1, CV_64F);
  cv::Mat tvec(3, 1, CV_64F);

  if (Ps.size() < 4) {
    SM_DEBUG_STREAM(
        "At least 4 points are needed for calling PnP. Found " << Ps.size());
    return false;
  }

  // Call the OpenCV pnp function.
  SM_DEBUG_STREAM(
      "Calling solvePnP with " << Ps.size() << " world points and " << Ms.size()
          << " image points");
  cv::solvePnP(Ps, Ms, cv::Mat::eye(3, 3, CV_64F), distCoeffs, rvec, tvec);

  // convert the rvec/tvec to a transformation
  cv::Mat C_camera_model = cv::Mat::eye(3, 3, CV_64F);
  Eigen::Matrix4d T_camera_model = Eigen::Matrix4d::Identity();
  cv::Rodrigues(rvec, C_camera_model);
  for (int r = 0; r < 3; ++r) {
    T_camera_model(r, 3) = tvec.at<double>(r, 0);
    for (int c = 0; c < 3; ++c) {
      T_camera_model(r, c) = C_camera_model.at<double>(r, c);
    }
  }

  out_T_t_c.set(T_camera_model.inverse());

  SM_DEBUG_STREAM("solvePnP solution:" << out_T_t_c.T());

  return true;
}

}  // namespace cameras

}  // namespace aslam
