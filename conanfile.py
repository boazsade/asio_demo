from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class ImageServerRecipe(ConanFile):
    name = "json-parser"
    version = "1.0"
    package_type = "library"

    # Optional metadata
    license = "The MIT License (MIT)"
    author = "boaz1sade@gmail.com"
    url = "https://github.com/boazsade/json_parser"
    description = "A JSON parser that enables direct conversion from and to C++ data types"
    topics = ("C++", "parser", "JSON")
    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("glog/0.7.0")
        self.requires("boost/1.85.0")
        
    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.user_presets_path = 'ConanPresets.json'
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
