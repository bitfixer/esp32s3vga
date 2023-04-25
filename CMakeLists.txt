# esp32s3vga library
# https://github.com/bitfixer/esp32s3vga
# GPLv3 License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "src/VGA.cpp"
                       INCLUDE_DIRS "src"
                       REQUIRES esp_lcd)

project(esp32s3vga)
