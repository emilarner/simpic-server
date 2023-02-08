#include <iostream>
#include <vector>

#include "../utils.hpp"

int main(int argc, char **argv, char **envp)
{
    std::string first_dir(argv[1]);
    std::string second_dir(argv[2]);

    std::vector<std::string> first_tokens = SimpicServerLib::split_string(first_dir, "/");
    std::vector<std::string> second_tokens = SimpicServerLib::split_string(second_dir, "/");

    for (int i = 0; i < std::min(first_tokens.size(), second_tokens.size()); i++)
    {
        std::cout << "First: " << first_tokens[i];
        std::cout << ", Second: " << second_tokens[i] << std::endl;
    }

    std::cout << "The second directory is a child of the first: ";
    std::cout << (SimpicServerLib::dir_is_child(first_dir, second_dir) ? "true" : "false");
    std::cout << std::endl;
}