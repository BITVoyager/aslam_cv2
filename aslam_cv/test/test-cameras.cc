#include <Eigen/Core>
#include <glog/logging.h>
#include <gtest/gtest.h>

#include <aslam/cameras/camera.h>
#include <aslam/cameras/camera-pinhole.h>
#include <aslam/cameras/distortion.h>
#include <aslam/cameras/distortion-fisheye.h>
#include <aslam/cameras/distortion-radtan.h>
#include <aslam/cameras/distortion-equidistant.h>
#include <aslam/common/eigen-helpers.h>
#include <aslam/common/eigen-predicates.h>
#include <aslam/common/entrypoint.h>
#include <aslam/common/memory.h>

///////////////////////////////////////////////
// Types to test
///////////////////////////////////////////////
using testing::Types;
typedef Types<aslam::PinholeCamera> Implementations;

///////////////////////////////////////////////
// Test fixture
///////////////////////////////////////////////
template <class CameraType>
class TestCameras : public testing::Test {
 protected:
  TestCameras() : camera_(CameraType::createTestCamera()) {};
  virtual ~TestCameras() {};
  typename CameraType::Ptr camera_;
};

TYPED_TEST_CASE(TestCameras, Implementations);

///////////////////////////////////////////////
// Generic test cases (run for all models)
///////////////////////////////////////////////
TYPED_TEST(TestCameras, CameraTest_EuclideanToOnAxisKeypoint) {
  Eigen::Vector3d euclidean(0, 0, 1);
  Eigen::Vector2d keypoint;
  this->camera_->project3(euclidean, &keypoint);

  Eigen::Vector2d image_center(this->camera_->cu(), this->camera_->cv());
  EXPECT_NEAR_EIGEN(image_center, keypoint, 1e-15);
}

TYPED_TEST(TestCameras, CameraTest_isVisible) {
  const double ru = this->camera_->imageWidth();
  const double rv = this->camera_->imageHeight();
  const double cu = this->camera_->cu();
  const double cv = this->camera_->cv();

  Eigen::Vector2d keypoint1(0, 0);
  EXPECT_TRUE(this->camera_->isKeypointVisible(keypoint1)) << "Keypoint1: " << keypoint1;

  Eigen::Vector2d keypoint2(ru - 1, rv - 1);
  EXPECT_TRUE(this->camera_->isKeypointVisible(keypoint2)) << "Keypoint2: " << keypoint2;

  Eigen::Vector2d keypoint3(cu, cv);
  EXPECT_TRUE(this->camera_->isKeypointVisible(keypoint3)) << "Keypoint3: " << keypoint3;

  Eigen::Vector2d keypoint4(-1, 0);
  EXPECT_FALSE(this->camera_->isKeypointVisible(keypoint4)) << "Keypoint4: " << keypoint4;

  Eigen::Vector2d keypoint5(-1, -1);
  EXPECT_FALSE(this->camera_->isKeypointVisible(keypoint5)) << "Keypoint5: " << keypoint5;

  Eigen::Vector2d keypoint6(ru, rv);
  EXPECT_FALSE(this->camera_->isKeypointVisible(keypoint6)) << "Keypoint6: " << keypoint6;
}

TYPED_TEST(TestCameras, CameraTest_isProjectable) {
  EXPECT_TRUE(this->camera_->isProjectable3(Eigen::Vector3d(0, 0, 1)));       // Center.
  EXPECT_FALSE(this->camera_->isProjectable3(Eigen::Vector3d(5, -5, 1)));     // In front of cam.
  EXPECT_FALSE(this->camera_->isProjectable3(Eigen::Vector3d(5000, -5, 1)));  // In front of cam, outside range.
  EXPECT_FALSE(this->camera_->isProjectable3(Eigen::Vector3d(-10, -10, -1))); // Behind cam.
  EXPECT_FALSE(this->camera_->isProjectable3(Eigen::Vector3d(0, 0, -1)));     // Behind, center.
}
TYPED_TEST(TestCameras, CameraTest_isInvertible) {
  const int N = 100;
  const double depth = 10.0;
  Eigen::Matrix3Xd points1(3,N);
  Eigen::Matrix2Xd projections1(2,N);
  Eigen::Matrix3Xd points2(3,N);
  Eigen::Matrix3Xd points3(3,N);
  Eigen::Matrix2Xd projections3(2,N);
  Eigen::Vector3d point;
  Eigen::Vector2d keypoint;

  // N times, project and back-project a random point at a known depth.
  // Then check that the back projection matches the projeciton.
  for(size_t n = 0; n < N; ++n) {
    points1.col(n) = this->camera_->createRandomVisiblePoint(depth);
    aslam::ProjectionResult result = this->camera_->project3(points1.col(n), &keypoint);
    projections1.col(n) = keypoint;
    ASSERT_EQ(aslam::ProjectionResult::Status::KEYPOINT_VISIBLE, result.getDetailedStatus());
    bool success = this->camera_->backProject3(keypoint, &point);
    ASSERT_TRUE(success);
    point.normalize();
    points2.col(n) = point * depth;
  }
  ASSERT_TRUE(aslam::common::MatricesEqual(points1, points2, 1e-4));

  // Do the same with the vectorized functions.
  std::vector<aslam::ProjectionResult> result;
  this->camera_->project3Vectorized(points1, &projections3, &result);
  for(size_t n = 0; n < N; ++n) {
    ASSERT_EQ(aslam::ProjectionResult::Status::KEYPOINT_VISIBLE, result[n].getDetailedStatus());
  }
  std::vector<bool> success;
  this->camera_->backProject3Vectorized(projections3, &points3, &success);
  for(size_t n = 0; n < N; ++n) {
    ASSERT_TRUE(success[n]);
    points3.col(n).normalize();
    points3.col(n) *= depth;
  }

  ASSERT_TRUE(aslam::common::MatricesEqual(points1, points3, 1e-4));
}

///////////////////////////////////////////////
// Model specific test cases
///////////////////////////////////////////////
TEST(CameraComparison, TestEquality) {
  using namespace aslam;
  Eigen::VectorXd dvec(4);

  dvec << 0.5, 0.3, 0.2, 0.01;
  Distortion::Ptr distortion_A = std::make_shared<RadTanDistortion>(dvec);
  Camera::Ptr camera_A = std::make_shared<PinholeCamera>(240, 480, 100, 200, 500, 500, distortion_A);

  dvec << 0.0, 0.3, 0.2, 0.01;
  Distortion::Ptr distortion_B = std::make_shared<RadTanDistortion>(dvec);
  Camera::Ptr camera_B = std::make_shared<PinholeCamera>(240, 480, 100, 200, 500, 500, distortion_B);

  dvec << 0.5, 0.3, 0.2, 0.01;
  Distortion::Ptr distortion_C = std::make_shared<RadTanDistortion>(dvec);
  Camera::Ptr camera_C = std::make_shared<PinholeCamera>(11111, 480, 100, 200, 500, 500, distortion_C);

  EXPECT_TRUE( *camera_A == *camera_A );  // Same camera, should be equal.
  EXPECT_FALSE( *camera_A == *camera_B ); // Different distortion, should be different.
  EXPECT_FALSE( *camera_A == *camera_C ); // Different intrinsics, should be different.
}

TEST(CameraComparison, TestStatus) {
  using namespace aslam;
  Eigen::VectorXd dvec(4);

  dvec << 0.5, 0.3, 0.2, 0.01;
  Distortion::Ptr distortion = std::make_shared<RadTanDistortion>(dvec);
  Camera::Ptr camera = std::make_shared<PinholeCamera>(240, 480, 100, 200, 500, 500, distortion);

  Eigen::Matrix<double, 3, 1> point(0, 0, -1);
  Eigen::Matrix<double, 2, 1> keypoint;
  ProjectionResult result = camera->project3(point, &keypoint);
  EXPECT_TRUE(result == ProjectionResult::POINT_BEHIND_CAMERA);
}
