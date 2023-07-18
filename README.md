# Tesseract
Source code for the chess engine used in the thesis. Developed with Visual Studio C++ and requires C++20 with BMI2 support for the relevant PEXT/PDEP instructions. Compiles with no warnings at level 4 (/W4).

Relevant settings in Visual Studio to get maximum performance:
- Optimization: "Maximum Optimization (Favor Speed) (/O2)"
- Favor Size Or Speed: "Favor fast code (/Ot)"
- Enable Incremental Linking: "No (/INCREMENTAL:NO)"
- Link Time Code Generation: "Use Link Time Code Generation (/LTCG)"

Note: The Perft testing done in the thesis was using a cleaned up move generator without any other overhead such as zobrist and evaluation calculations. Due to this the move generator is slower in the final engine with all the bells and whistles attached. Furthermore, this is not the exact version that was used for testing, and as such the STS results might slightly deviate from the numbers in the thesis. The performance against other engines is still largely the same
