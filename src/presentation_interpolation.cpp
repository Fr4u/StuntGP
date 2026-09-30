#include "sgn/presentation_interpolation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace sgn
{
namespace
{
using Matrix = std::array<float, 16>;
using DMatrix = std::array<double, 16>;
using DVector = std::array<double, 3>;
using Quaternion = std::array<double, 4>;
constexpr DMatrix identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
double dot3(DVector a, DVector b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
DVector times(DVector a, double b)
{
    return {a[0] * b, a[1] * b, a[2] * b};
}
DVector minus(DVector a, DVector b)
{
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
DVector cross3(DVector a, DVector b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
DVector normalized(DVector v)
{
    return times(v, 1 / std::sqrt(dot3(v, v)));
}
DVector perpendicular(DVector v)
{
    const auto axis =
        std::min_element(v.begin(), v.end(), [](double a, double b) { return std::abs(a) < std::abs(b); }) - v.begin();
    DVector basis{};
    basis[axis] = 1;
    return normalized(minus(basis, times(v, dot3(basis, v))));
}
DMatrix widen(const Matrix &matrix)
{
    DMatrix result;
    std::copy(matrix.begin(), matrix.end(), result.begin());
    return result;
}
Matrix narrow(const DMatrix &matrix)
{
    Matrix result;
    std::transform(matrix.begin(), matrix.end(), result.begin(), [](double x) {
        if (!std::isfinite(x) || std::abs(x) > std::numeric_limits<float>::max())
            throw FormatError("nonfinite presentation matrix");
        return static_cast<float>(x);
    });
    return result;
}
DMatrix multiply(const DMatrix &a, const DMatrix &b)
{
    DMatrix result{};
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 4; ++col)
            for (unsigned k = 0; k < 4; ++k)
                result[row * 4 + col] += a[row * 4 + k] * b[k * 4 + col];
    return result;
}
DMatrix inverse(const DMatrix &m)
{
    const DVector a{m[0], m[1], m[2]}, b{m[4], m[5], m[6]}, c{m[8], m[9], m[10]};
    const auto first = cross3(b, c), second = cross3(c, a), third = cross3(a, b);
    const auto determinant = dot3(a, first);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-15)
        throw FormatError("singular presentation camera matrix");
    auto result = identity;
    for (unsigned i = 0; i < 3; ++i)
    {
        result[i * 4] = first[i] / determinant;
        result[i * 4 + 1] = second[i] / determinant;
        result[i * 4 + 2] = third[i] / determinant;
    }
    for (unsigned i = 0; i < 3; ++i)
        result[12 + i] = -(m[12] * result[i] + m[13] * result[4 + i] + m[14] * result[8 + i]);
    return result;
}
Quaternion normalize_quaternion(Quaternion q)
{
    double norm = 0;
    for (double x : q)
        norm += x * x;
    for (double &x : q)
        x /= std::sqrt(norm);
    return q;
}
Quaternion rotation_quaternion(const std::array<DVector, 3> &row)
{
    // Standard quaternion extraction uses column-vector matrices: transpose
    // our row-vector basis at this boundary, retaining a proper rotation.
    const auto r = [&](unsigned i, unsigned j) { return row[j][i]; };
    Quaternion q{};
    const double trace = r(0, 0) + r(1, 1) + r(2, 2);
    if (trace > 0)
    {
        const double s = std::sqrt(trace + 1) * 2;
        q = {(r(2, 1) - r(1, 2)) / s, (r(0, 2) - r(2, 0)) / s, (r(1, 0) - r(0, 1)) / s, s / 4};
    }
    else
    {
        unsigned i = 0;
        if (r(1, 1) > r(i, i))
            i = 1;
        if (r(2, 2) > r(i, i))
            i = 2;
        const unsigned j = (i + 1) % 3, k = (i + 2) % 3;
        const double s = std::sqrt(std::max(0.0, 1 + r(i, i) - r(j, j) - r(k, k))) * 2;
        q[i] = s / 4;
        q[j] = (r(j, i) + r(i, j)) / s;
        q[k] = (r(k, i) + r(i, k)) / s;
        q[3] = (r(k, j) - r(j, k)) / s;
    }
    return normalize_quaternion(q);
}
std::array<DVector, 3> quaternion_rows(Quaternion q)
{
    const auto [x, y, z, w] = q;
    return {{{1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w)},
             {2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w)},
             {2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)}}};
}
Quaternion slerp(Quaternion a, Quaternion b, double alpha)
{
    double cosine = 0;
    for (unsigned i = 0; i < 4; ++i)
        cosine += a[i] * b[i];
    if (cosine < 0)
    {
        for (double &x : b)
            x = -x;
        cosine = -cosine;
    }
    cosine = std::clamp(cosine, 0.0, 1.0);
    double wa = 1 - alpha, wb = alpha;
    if (cosine < 0.9995)
    {
        const double angle = std::acos(cosine), divisor = std::sin(angle);
        wa = std::sin((1 - alpha) * angle) / divisor;
        wb = std::sin(alpha * angle) / divisor;
    }
    Quaternion result{};
    for (unsigned i = 0; i < 4; ++i)
        result[i] = wa * a[i] + wb * b[i];
    return normalize_quaternion(result);
}
struct Affine
{
    Quaternion rotation{};
    std::array<DVector, 3> residual{};
    DVector position{};
};
Affine decompose(const DMatrix &m)
{
    const std::array<DVector, 3> rows{{{m[0], m[1], m[2]}, {m[4], m[5], m[6]}, {m[8], m[9], m[10]}}};
    double scale = 0;
    for (const auto &row : rows)
        scale = std::max(scale, dot3(row, row));
    const double threshold = std::max(1e-28, scale * 1e-24);
    DVector x = rows[0];
    if (dot3(x, x) <= threshold)
    {
        x = cross3(rows[1], rows[2]);
        if (dot3(x, x) <= threshold)
        {
            const auto strongest = dot3(rows[1], rows[1]) > dot3(rows[2], rows[2]) ? rows[1] : rows[2];
            x = dot3(strongest, strongest) > threshold ? perpendicular(normalized(strongest)) : DVector{1, 0, 0};
        }
    }
    x = normalized(x);
    DVector y = minus(rows[1], times(x, dot3(rows[1], x)));
    if (dot3(y, y) <= threshold)
    {
        y = cross3(rows[2], x);
        if (dot3(y, y) <= threshold)
            y = perpendicular(x);
    }
    y = normalized(y);
    const std::array<DVector, 3> rotation{x, y, cross3(x, y)};
    Affine result;
    result.rotation = rotation_quaternion(rotation);
    result.position = {m[12], m[13], m[14]};
    // A full residual preserves shear, negative scales and rank-deficient
    // transforms. No source matrix is orthonormalized or written back.
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            result.residual[i][j] = dot3(rows[i], rotation[j]);
    return result;
}
DMatrix interpolate(const Affine &a, const Affine &b, double alpha)
{
    const auto rotation = quaternion_rows(slerp(a.rotation, b.rotation, alpha));
    auto result = identity;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
        {
            result[i * 4 + j] = 0;
            for (unsigned k = 0; k < 3; ++k)
                result[i * 4 + j] += std::lerp(a.residual[i][k], b.residual[i][k], alpha) * rotation[k][j];
        }
    for (unsigned i = 0; i < 3; ++i)
        result[12 + i] = std::lerp(a.position[i], b.position[i], alpha);
    return result;
}
DMatrix pose_matrix(const Pose &p)
{
    return {p.right.x * p.scale,   p.right.y * p.scale,   p.right.z * p.scale,   0,
            p.up.x * p.scale,      p.up.y * p.scale,      p.up.z * p.scale,      0,
            p.forward.x * p.scale, p.forward.y * p.scale, p.forward.z * p.scale, 0,
            p.position.x,          p.position.y,          p.position.z,          1};
}
Pose matrix_pose(const DMatrix &d)
{
    const auto m = narrow(d);
    return {{m[0], m[1], m[2]}, {m[4], m[5], m[6]}, {m[8], m[9], m[10]}, {m[12], m[13], m[14]}, 1};
}
void validate_matrix(const Matrix &m)
{
    for (float v : m)
        if (!std::isfinite(v))
            throw FormatError("snapshot contains nonfinite matrix");
    if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
        throw FormatError("snapshot transform is not affine");
}
void validate_view(const View &view)
{
    for (const auto v : {view.eye, view.target, view.up})
        for (float x : {v.x, v.y, v.z})
            if (!std::isfinite(x))
                throw FormatError("snapshot contains nonfinite camera vector");
    for (float x : {view.near_plane, view.far_plane})
        if (!std::isfinite(x))
            throw FormatError("snapshot contains nonfinite clip plane");
    if (view.source_projection)
        for (float x : *view.source_projection)
            if (!std::isfinite(x))
                throw FormatError("snapshot contains nonfinite projection");
    if (view.retail_viewport)
    {
        const auto &p = view.retail_viewport->projection;
        for (float x : {p.x, p.y, p.z, p.offset_z, p.clip_near, view.retail_viewport->clip_far})
            if (!std::isfinite(x))
                throw FormatError("snapshot contains nonfinite viewport");
    }
}
float blend(float a, float b, double alpha)
{
    return static_cast<float>(std::lerp(double(a), double(b), alpha));
}
Vec3 blend(Vec3 a, Vec3 b, double alpha)
{
    return {blend(a.x, b.x, alpha), blend(a.y, b.y, alpha), blend(a.z, b.z, alpha)};
}
View blend_view(const View &a, const View &b, const Matrix &matrix, double alpha)
{
    auto result = b;
    result.world_to_view = matrix;
    result.eye = blend(a.eye, b.eye, alpha);
    result.target = blend(a.target, b.target, alpha);
    result.up = blend(a.up, b.up, alpha);
    result.near_plane = blend(a.near_plane, b.near_plane, alpha);
    result.far_plane = blend(a.far_plane, b.far_plane, alpha);
    if (result.source_projection)
        for (unsigned i = 0; i < 16; ++i)
            (*result.source_projection)[i] = blend((*a.source_projection)[i], (*b.source_projection)[i], alpha);
    if (result.retail_viewport)
    {
        auto &out = *result.retail_viewport;
        const auto &x = *a.retail_viewport;
        const auto &y = *b.retail_viewport;
        out.projection = {blend(x.projection.x, y.projection.x, alpha), blend(x.projection.y, y.projection.y, alpha),
                          blend(x.projection.z, y.projection.z, alpha),
                          blend(x.projection.offset_z, y.projection.offset_z, alpha),
                          blend(x.projection.clip_near, y.projection.clip_near, alpha)};
        out.clip_far = blend(x.clip_far, y.clip_far, alpha);
    }
    return result;
}
} // namespace
struct PresentationInterpolator::Impl
{
    struct Pair
    {
        const PresentationObject *previous{};
        std::vector<std::pair<Affine, Affine>> parts;
        std::pair<Affine, Affine> pose;
    };
    ImmutablePresentationSnapshot previous, current;
    std::vector<Pair> pairs;
    std::pair<Affine, Affine> camera;
    bool history_valid{}, invalidate_next{true};
    PresentationDraw draw;
    PresentationInterpolationStats statistics;
};
PresentationInterpolator::PresentationInterpolator() : impl_(std::make_unique<Impl>()) {}
PresentationInterpolator::~PresentationInterpolator() = default;
void PresentationInterpolator::publish(ImmutablePresentationSnapshot frame)
{
    if (!frame || !frame->view.world_to_view)
        throw FormatError("presentation snapshot requires an owned source camera view");
    validate_view(frame->view);
    validate_matrix(*frame->view.world_to_view);
    const auto camera_current = inverse(widen(*frame->view.world_to_view));
    std::map<std::uint64_t, const PresentationObject *> ids;
    for (const auto &object : frame->objects)
    {
        if (!ids.emplace(object.identity, &object).second)
            throw FormatError("duplicate presentation object identity");
        if (!object.geometry || object.parts.size() != object.model_to_view.size() ||
            object.parts.size() != object.geometry->model.parts)
            throw FormatError("presentation object storage mismatch");
        if (!valid_pose(object.pose))
            throw FormatError("invalid snapshot pose");
        for (const auto &part : object.parts)
            if (!valid_pose(part.pose))
                throw FormatError("invalid snapshot part pose");
        for (const auto &matrix : object.model_to_view)
            validate_matrix(matrix);
    }
    auto &w = *impl_;
    if (w.current && frame->tick <= w.current->tick)
        throw FormatError("presentation tick did not advance");
    w.previous = w.current;
    w.current = std::move(frame);
    w.history_valid = !w.invalidate_next && w.previous && w.current->tick - w.previous->tick == 1 &&
                      w.current->camera_continuity == w.previous->camera_continuity &&
                      w.current->view.source_projection.has_value() == w.previous->view.source_projection.has_value() &&
                      w.current->view.retail_viewport.has_value() == w.previous->view.retail_viewport.has_value();
    if (w.history_valid && w.current->view.retail_viewport)
        w.history_valid =
            w.current->view.retail_viewport->widen_horizontal == w.previous->view.retail_viewport->widen_horizontal;
    w.invalidate_next = false;
    ++w.statistics.publications;
    if (w.previous && !w.history_valid)
        ++w.statistics.camera_cuts;
    w.draw.objects = w.current->objects;
    w.draw.instances.resize(w.current->objects.size());
    w.draw.live_views.resize(w.current->objects.size());
    w.pairs.clear();
    w.pairs.resize(w.current->objects.size());
    if (w.history_valid)
    {
        const auto camera_previous = inverse(widen(*w.previous->view.world_to_view));
        w.camera = {decompose(camera_previous), decompose(camera_current)};
        ids.clear();
        for (const auto &object : w.previous->objects)
            ids.emplace(object.identity, &object);
        for (std::size_t i = 0; i < w.current->objects.size(); ++i)
        {
            const auto &object = w.current->objects[i];
            auto &pair = w.pairs[i];
            const auto found = ids.find(object.identity);
            if (found == ids.end() || found->second->continuity != object.continuity ||
                found->second->mesh != object.mesh || found->second->parts.size() != object.parts.size())
            {
                ++w.statistics.object_cuts;
                continue;
            }
            pair.previous = found->second;
            pair.pose = {decompose(pose_matrix(pair.previous->pose)), decompose(pose_matrix(object.pose))};
            pair.parts.reserve(object.parts.size());
            for (unsigned part = 0; part < object.parts.size(); ++part)
                pair.parts.emplace_back(decompose(multiply(widen(pair.previous->model_to_view[part]), camera_previous)),
                                        decompose(multiply(widen(object.model_to_view[part]), camera_current)));
        }
    }
}
void PresentationInterpolator::invalidate_history() noexcept
{
    impl_->history_valid = false;
    impl_->invalidate_next = true;
    ++impl_->statistics.history_resets;
}
const PresentationDraw &PresentationInterpolator::sample(double alpha)
{
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
        throw FormatError("presentation alpha outside 0..1");
    auto &w = *impl_;
    ++w.statistics.samples;
    if (!w.current)
        return w.draw;
    const bool interpolation = w.history_valid && alpha < 1;
    const bool middle = interpolation && alpha > 0;
    if (middle)
        ++w.statistics.intermediate_samples;
    DMatrix view_matrix = widen(*w.current->view.world_to_view);
    if (interpolation)
    {
        view_matrix = alpha == 0 ? widen(*w.previous->view.world_to_view)
                                 : inverse(interpolate(w.camera.first, w.camera.second, alpha));
        w.draw.view =
            alpha == 0 ? w.previous->view : blend_view(w.previous->view, w.current->view, narrow(view_matrix), alpha);
    }
    else
        w.draw.view = w.current->view;
    const auto current_camera = inverse(widen(*w.current->view.world_to_view));
    for (std::size_t i = 0; i < w.current->objects.size(); ++i)
    {
        const auto &current = w.current->objects[i];
        const auto &pair = w.pairs[i];
        auto &object = w.draw.objects[i];
        object.pose = current.pose;
        if (interpolation && pair.previous)
            object.pose =
                alpha == 0 ? pair.previous->pose : matrix_pose(interpolate(pair.pose.first, pair.pose.second, alpha));
        for (unsigned part = 0; part < current.parts.size(); ++part)
        {
            object.parts[part] = current.parts[part];
            if (interpolation && pair.previous && pair.previous->parts[part].visible == current.parts[part].visible)
            {
                object.model_to_view[part] =
                    alpha == 0 ? pair.previous->model_to_view[part]
                               : narrow(multiply(interpolate(pair.parts[part].first, pair.parts[part].second, alpha),
                                                 view_matrix));
                if (middle)
                    ++w.statistics.interpolated_parts;
            }
            else if (interpolation)
            {
                // A discrete object/LOD cut uses the current world transform
                // viewed through the interpolated camera, not an obsolete mesh.
                object.model_to_view[part] =
                    narrow(multiply(multiply(widen(current.model_to_view[part]), current_camera), view_matrix));
            }
            else
                object.model_to_view[part] = current.model_to_view[part];
        }
        w.draw.instances[i] = {object.mesh, object.pose, object.parts};
        w.draw.live_views[i] = {object.model_to_view};
    }
    return w.draw;
}
const PresentationInterpolationStats &PresentationInterpolator::stats() const noexcept
{
    return impl_->statistics;
}
ImmutablePresentationSnapshot PresentationInterpolator::current() const noexcept
{
    return impl_->current;
}
} // namespace sgn
