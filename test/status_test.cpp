#include "axle/status.h"

#include <cstdint>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace axle {

TEST(StatusTest, Ok) {
    Status status = Ok(-2);

    ASSERT_TRUE(status.is_ok());
    ASSERT_EQ(-2, status.ok());
    ASSERT_FALSE(status.is_err());
}

TEST(StatusTest, Error) {
    Status status = Err("error message");

    ASSERT_TRUE(status.is_err());
    ASSERT_EQ(std::string("error message"), status.err());
    ASSERT_FALSE(status.is_ok());
}

template <typename T>
struct ComplexResult {
    std::unique_ptr<T> result;
    std::vector<char> payload;
};

enum class ErrorCode : uint8_t {
    NOT_FOUND,
    INVALID,
    UNKNOWN,
};

TEST(StatusTest, ComplexOk) {
    const std::vector<char> payload = {'a', 'c', 'b'};
    const int result_expected = 23;
    ComplexResult<int> res{std::make_unique<int>(result_expected), payload};
    Status status = Ok(std::move(res));

    ASSERT_TRUE(status.is_ok());
    ASSERT_FALSE(status.is_err());

    const ComplexResult<int> cplx_res = status.ok();
    ASSERT_EQ(result_expected, *cplx_res.result);
    ASSERT_EQ(payload, cplx_res.payload);
}

TEST(StatusTest, ComplexError) {
    const std::vector<char> payload = {'a', 'c', 'b'};
    ComplexResult<ErrorCode> res{std::make_unique<ErrorCode>(ErrorCode::INVALID), payload};
    Status status = Err(std::move(res));

    ASSERT_TRUE(status.is_err());
    ASSERT_FALSE(status.is_ok());

    const ComplexResult<ErrorCode> cplx_res = status.err();
    ASSERT_EQ(ErrorCode::INVALID, *cplx_res.result);
    ASSERT_EQ(payload, cplx_res.payload);
}

TEST(StatusTest, NoneOk) {
    const Status status = Ok();

    ASSERT_TRUE(status.is_ok());
    ASSERT_FALSE(status.is_err());
}

TEST(StatusTest, NoneErr) {
    const Status status = Err();

    ASSERT_TRUE(status.is_err());
    ASSERT_FALSE(status.is_ok());
}

TEST(StatusTest, OkConversion) {
    const Status status = Ok();
    ASSERT_TRUE(status.is_ok());
}

TEST(StatusTest, OkConversionComplex) {
    const int num = 42;
    std::unique_ptr<int> ptr = std::make_unique<int>(num);
    Status status = Ok(std::move(ptr));
    ASSERT_TRUE(status.is_ok());
    ASSERT_EQ(num, *status.ok().get());
}

TEST(StatusTest, ErrConversion) {
    const Status status = Err();
    ASSERT_TRUE(status.is_err());
}

TEST(StatusTest, ErrConversionComplex) {
    const int num = 42;
    std::unique_ptr<int> ptr = std::make_unique<int>(num);

    Status status = Err(std::move(ptr));
    ASSERT_TRUE(status.is_err());
    ASSERT_EQ(num, *status.err().get());
}

} // namespace axle
