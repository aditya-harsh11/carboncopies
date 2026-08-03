#include <sycl/sycl.hpp>
#include <iostream>

int main() {
    // In-order queue so later kernels wait for earlier ones automatically
    sycl::queue q{ sycl::property::queue::in_order{} };

    std::cout << "Running on: "
              << q.get_device().get_info<sycl::info::device::name>() << "\n";

    const int N = 5;

    // USM allocations (shared => accessible from host & device)
    int*   data   = sycl::malloc_shared<int>(N, q);
    float* result = sycl::malloc_device<float>(N, q);
    float* buf2   = sycl::malloc_shared<float>(N * 2, q);
    float* buf3   = sycl::malloc_shared<float>(N, q);

    // Initialize on host
    int init_vals[N] = {1, 2, 3, 4, 5};
    for (int i = 0; i < N; ++i) {
        data[i]   = init_vals[i];
    }
    for (int i = 0; i < 2 * N; ++i) buf2[i] = 0.0f;
    for (int i = 0; i < N; ++i)     buf3[i] = 0.0f;

    // --- Kernel 1: result[i] = sin(data[i])
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
        int i = idx[0];
        result[i] = sycl::sin(static_cast<float>(data[i]));
    });

    // --- Kernel 2: duplicate each result element into buf2 (at 2*i and 2*i+1)
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
        int i = idx[0];
        float v = result[i];
        buf2[2 * i]     = v;
        buf2[2 * i + 1] = v;
    });

    // --- Kernel 3: write into buf3 with an offset ( (i+2) % N )
    q.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
        int i = idx[0];
        float v = result[i];
        buf3[(i + 2) % N] = v + 2.0f;
    });

    // Ensure all kernels completed before reading on host
    q.wait();

    // Print results from buf3
    std::cout << "Results: ";
    for (int i = 0; i < N; ++i) {
        std::cout << buf3[i] << " ";
    }
    std::cout << "\n";

    // Free USM
    sycl::free(data, q);
    sycl::free(result, q);
    sycl::free(buf2, q);
    sycl::free(buf3, q);
    return 0;
}