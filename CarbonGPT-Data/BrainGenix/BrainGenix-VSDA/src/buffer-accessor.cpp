#include <sycl/sycl.hpp>
#include <iostream>

int main() {
    // Create a SYCL queue (automatically selects available device)
    sycl::queue q;
    
    // Print the device being used
    std::cout << "Running on: " 
              << q.get_device().get_info<sycl::info::device::name>() 
              << "\n";
    
    // Simple computation: square numbers using SYCL
    const int N = 5;
    int data[N] = {1, 2, 3, 4, 5};
    float result[N] = {0};
    
    // Create a SYCL buffer for the data
    sycl::buffer<int, 1> buf_data(data, sycl::range<1>(N));
    sycl::buffer<float, 1> buf_result(result, sycl::range<1>(N));
    sycl::buffer<float, 1> buf2(sycl::range<1>(N*2));
    sycl::buffer<float, 1> buf3((sycl::range<1>(N)));
    
    // Submit work to the queue
    q.submit([&](sycl::handler& h) {
        // Create accessors to access the buffers
        auto acc_data = buf_data.get_access<sycl::access::mode::read>(h); 
        auto acc_result = buf_result.get_access<sycl::access::mode::write>(h);
        
        // Execute parallel computation
        h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
            int i = idx[0];
            acc_result[i] = sycl::sin(float(acc_data[i]));//acc_data[i] * acc_data[i];  //calc cosine
        });
    });
    
    q.submit([&](sycl::handler& h) {
        auto read_buf_result = buf_result.get_access<sycl::access::mode::read>(h); 
        auto write_buf_2 = buf2.get_access<sycl::access::mode::write>(h);

        // [=] means capture variables (like accessors) by value so the kernel can use them on the device.
        h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {

            // get this thread id
            int this_thread = idx[0];

            // read result from input buffer
            float input = read_buf_result[this_thread];

            // write to result buffer twice
            write_buf_2[this_thread * 2] = input;
            write_buf_2[this_thread * 2 + 1] = input;
        });
    });

    q.submit([&](sycl::handler& h) {
        auto read_buf_result = buf_result.get_access<sycl::access::mode::read>(h); 
        auto write_buf3 = buf3.get_access<sycl::access::mode::write>(h);

        h.parallel_for(sycl::range<1>(N), [=](sycl::id<1> idx) {
            // get thread id
            int thread_id = idx[0];
            float input = read_buf_result[thread_id];

            // write to res buffer
            write_buf3[(thread_id + 2) % N] = input + 2;
        });
    });

    // Wait for computation to complete and access results
    auto host_acc = buf3.get_access<sycl::access::mode::read>();
    
    // Print results
    std::cout << "Results: ";
    for (int i = 0; i < N; i++) {
        std::cout << host_acc[i] << " ";
    }
    std::cout << "\n";
    
    return 0;
}