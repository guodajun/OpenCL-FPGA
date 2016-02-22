#ifndef CONVOLUTION_HEADER
#define CONVOLUTION_HEADER

#include "layer.hpp"

#include <string>

namespace cnn {
    class ConvolutionLayer : public Layer {
    public:

        ConvolutionLayer(size_t iWidth, size_t iHeight, size_t iDepth,
            size_t kernelSize, size_t oDepth,
            const vec &weight, const vec &offset
            ) : Layer(iWidth, iHeight, iDepth, iWidth - kernelSize + 1, iHeight - kernelSize + 1, oDepth, weight, offset),
            kernelSize(kernelSize) {

            // Resize the output buffer.
            output.resize(oDepth * oWidth * oHeight);

            // Resize the input buffer.
            inputBuffer.resize(kernelSize * kernelSize);

            // Initialize OpenCL.
            initOpenCL();
        }

        // Forward.
        virtual void forward(const vec &in) {
            forwardCPU(in);
        }

        // Forward with CPU.
        void forwardCPU(const vec &in) {

            // Clear the output buffer.
            std::fill(output.begin(), output.end(), 0.0f);

            // For each output feature map.
            for (size_t o = 0; o < oDepth; ++o) {
                // For each input feature map.
                for (size_t i = 0; i < iDepth; ++i) {
                    // For each element in the output feature map.
                    for (size_t r = 0; r < oHeight; ++r) {
                        for (size_t c = 0; c < oWidth; ++c) {
                            getInput(i, r, c, in);
                            output[getOutputIdx(o, r, c)] += convolution(getWeightBase(i, o));
                        }
                    }
                }

                // Activate function.
                for (size_t r = 0; r < oHeight; ++r) {
                    for (size_t c = 0; c < oWidth; ++c) {
                        size_t idx = getOutputIdx(o, r, c);
                        output[idx] = sigmod(output[idx] + offset[o]);
                    }
                }
            }
        }

        // Forward with OpenCL on GPU.
        void forwardGPU(const vec &in) {

            cl_int err;

            // Allocate memory on device.
            cl_mem clIn = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                iWidth * iHeight * iDepth * sizeof(cl_float),
                const_cast<void *>(static_cast<const void *>(&in[0])),
                &err);

            cl_mem clWeight = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                kernelSize * kernelSize * iDepth * oDepth * sizeof(cl_float),
                const_cast<void *>(static_cast<const void *>(&weight[0])),
                &err);

            cl_mem clOffset = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                oDepth * sizeof(cl_float),
                const_cast<void *>(static_cast<const void *>(&offset[0])),
                &err);

            cl_mem clOut = clCreateBuffer(
                context,
                CL_MEM_WRITE_ONLY,
                oDepth * oHeight * oWidth * sizeof(cl_float),
                NULL,
                &err);

            if (err != CL_SUCCESS) {
                std::cerr << "Failed creating the buffer." << std::endl;
                std::cerr << readable_status(err);
                exit(-1);
            }

            // Set the arguments for the kernel.
            std::string kernelName = "forwardGPU";
            cl_kernel kernel = clCreateKernel(program, kernelName.c_str(), &err);
            err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &clIn);
            err = clSetKernelArg(kernel, 1, sizeof(cl_mem), &clWeight);
            err = clSetKernelArg(kernel, 2, sizeof(cl_mem), &clOffset);
            err = clSetKernelArg(kernel, 3, sizeof(cl_mem), &clOut);
            err = clSetKernelArg(kernel, 4, sizeof(cl_int), &iWidth);
            err = clSetKernelArg(kernel, 5, sizeof(cl_int), &iHeight);
            err = clSetKernelArg(kernel, 6, sizeof(cl_int), &iDepth);
            err = clSetKernelArg(kernel, 7, sizeof(cl_int), &oWidth);
            err = clSetKernelArg(kernel, 8, sizeof(cl_int), &oHeight);
            err = clSetKernelArg(kernel, 9, sizeof(cl_int), &oDepth);
            err = clSetKernelArg(kernel, 10, sizeof(cl_int), &kernelSize);

            // Prepare the NDRange.
            int items = 16;
            size_t global[] = {
                (size_t)closestMultiple(items, (int)oWidth),
                (size_t)closestMultiple(items, (int)(oDepth * oHeight))
            };
            size_t local[] = {
                (size_t)items,
                (size_t)items
            };

            cl_ulong t = runAndTimeKernel(queue, kernel, 2, global, local);
            err = clEnqueueReadBuffer(queue,
                clOut,
                CL_TRUE,
                0,
                oWidth * oHeight * oDepth * sizeof(cl_float),
                (void *)&output[0],
                0,
                NULL,
                NULL);
            if (err != CL_SUCCESS) {
                std::cerr << "Failed reading the output." << std::endl;
                std::cerr << readable_status(err);
                exit(-1);
            }
        }

        // Prepare the input buffer.
        inline void getInput(size_t i, size_t r, size_t c, const vec &in) {
            size_t idx = 0;
            for (size_t x = 0; x < kernelSize; ++x) {
                for (size_t y = 0; y < kernelSize; ++y) {
                    inputBuffer[idx++] = in[i * iWidth * iHeight + (r + x) * iWidth + c + y];
                }
            }
        }

        // Get the output feature map element index.
        inline size_t getOutputIdx(size_t o, size_t r, size_t c) {
            return o * oWidth * oHeight + r * oWidth + c;
        }

        // Get the base index of the weight.
        inline size_t getWeightBase(size_t i, size_t o) {
            return (o * iDepth + i) * kernelSize * kernelSize;
        }

        // Do the convolution with weight and the input buffer.
        float convolution(size_t weightBase) {
            float sum = 0.0f;
            for (size_t i = 0; i < kernelSize * kernelSize; ++i) {
                sum += weight[weightBase + i] * inputBuffer[i];
            }
            return sum;
        }

        size_t kernelSize;

        // Buffer for convolution.
        vec inputBuffer;

        // For OpenCL.
        cl_context context;
        cl_command_queue queue;
        cl_program program;

        // Initialize the OpenCL.
        void initOpenCL() {
            cl_platform_id platforms[2];
            cl_device_id devices[2];
            cl_int err;

            // Choose the first platform.
            err = clGetPlatformIDs(1, platforms, NULL);

            // Get the first GPU device.
            err = clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, 1, devices, NULL);

            cl_context_properties properties[] = {
                CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[0], 0
            };

            context = clCreateContext(
                properties,
                1,
                devices,
                NULL,
                NULL,
                &err);

            queue = clCreateCommandQueue(
                context,
                devices[0],
                CL_QUEUE_PROFILING_ENABLE,
                &err);

            program = buildProgram("convolution.cl", context, devices[0]);
        }
    };

    // Helper function to create a convolution layer from xml file.
    ConvolutionLayer createConvolutionLayerFromXML(const std::string &fn) {
        std::string str = fileToString(fn);
        char *text = new char[str.size() + 1];
        memcpy((void *)text, (void *)(&str[0]), str.size() * sizeof(char));
        text[str.size()] = '\0';

        // Parse the xml file.
        rapidxml::xml_document<> doc;
        doc.parse<0>(text);

        rapidxml::xml_node<> *root = doc.first_node("ConvolutionalLayer");

        auto getInt = [](rapidxml::xml_node<> *root, const char *name) -> int {
            rapidxml::xml_node<> *node = root->first_node(name);
            return std::stoi(node->value());
        };

        // Get the parameters for the convolutional layer.
        int iWidth = getInt(root, "iWidth");
        int iHeight = getInt(root, "iHeight");
        int iDepth = getInt(root, "iDepth");
        int kernelSize = getInt(root, "kernelSize");
        int oDepth = getInt(root, "oDepth");

        // Create the weight vector.
        cnn::vec weight;
        getAllItem(root->first_node("weight"), weight);
        assert(weight.size() == oDepth * iDepth * kernelSize * kernelSize);

        // Create the offset vector.
        cnn::vec offset;
        for (rapidxml::xml_node<> *node = root->first_node("offset")->first_node(); node; node = node->next_sibling()) {
            offset.push_back(std::stof(node->value()));
        }
        assert(offset.size() == oDepth);

        delete[] text;

        cnn::ConvolutionLayer layer(iWidth, iHeight, iDepth, kernelSize, oDepth, weight, offset);
        return layer;
    }
}

#endif