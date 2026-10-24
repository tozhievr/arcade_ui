import arcade.ui.test.contracts;
#include <string>
int main(int argc, char** argv) {
    if (argc < 2) return 99;
    return exercise_app(std::string(argv[1]));
}
