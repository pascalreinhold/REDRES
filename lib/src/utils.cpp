#include "utils.hpp"
#include <fstream>
#include <iostream>

namespace rcc {

vk::ImageCreateInfo imageCreateInfo(vk::Format format, vk::ImageUsageFlags usageFlags, vk::Extent3D extent) {
    return vk::ImageCreateInfo{
        vk::ImageCreateFlags(),
        vk::ImageType::e2D,
        format,
        extent,
        1,
        1,
        vk::SampleCountFlagBits::e1,
        vk::ImageTiling::eOptimal,
        usageFlags};
}

vk::ImageViewCreateInfo imageviewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags) {

    auto subresource_range = vk::ImageSubresourceRange{
        aspectFlags,
        0,
        1,
        0,
        1};

    return vk::ImageViewCreateInfo{
        vk::ImageViewCreateFlags(),
        image,
        vk::ImageViewType::e2D,
        format,
        vk::ComponentMapping{},
        subresource_range};
}

vk::SamplerCreateInfo samplerCreateInfo(vk::Filter filters, vk::SamplerAddressMode samplerAddressMode) {
    vk::SamplerCreateInfo info{};
    info.magFilter = filters;
    info.minFilter = filters;
    info.addressModeU = samplerAddressMode;
    info.addressModeV = samplerAddressMode;
    info.addressModeW = samplerAddressMode;
    return info;
}

glm::mat3 xyz_reader::getBasisFromString(const char *text) {
    glm::mat3 basis;
    sscanf(text, "%*9s %f %f %f %f %f %f %f %f %f",
           &basis[0][0], &basis[0][1], &basis[0][2],
           &basis[1][0], &basis[1][1], &basis[1][2],
           &basis[2][0], &basis[2][1], &basis[2][2]);
    return basis;
}

void xyz_reader::readFile(const std::string &filename, std::vector<structureFrameData> &data) {
    std::ifstream inputStream(filename);
    assert(inputStream.is_open() && "could not open xyz file");

    std::string line;
    while (std::getline(inputStream, line)) {
        if (line.empty()) continue;

        int numberOfAtoms = std::stoi(line);

        //create a new frame tuple
        data.emplace_back();

        //Create Unit Cell Basis From This
        auto &basis = std::get<2>(data.back());
        auto &positions = std::get<1>(data.back());
        auto &symbols = std::get<0>(data.back());

        std::getline(inputStream, line);
        basis = getBasisFromString(line.c_str());
        positions.resize(numberOfAtoms);
        symbols.resize(numberOfAtoms);

        for (int i = 0; i < numberOfAtoms; i++) {
            inputStream >> symbols[i].str >> positions[i][0] >> positions[i][1] >> positions[i][2];
        }
    }
}

void xyz_reader::printStructureData(int frameCount, const std::vector<structureFrameData> &data) {
    for (int i = 0; i < frameCount; i++) {
        auto &basis = std::get<2>(data[i]);
        auto &positions = std::get<1>(data[i]);
        auto &symbols = std::get<0>(data[i]);
        assert(positions.size()==symbols.size());

        for (int j = 0; j < positions.size(); j++) {
            std::cout << symbols[j].str << "\t[" << positions[j][0] << ", " << positions[j][1] << ", "
                      << positions[j][2]
                      << "]\n";
        }
    }
}

}