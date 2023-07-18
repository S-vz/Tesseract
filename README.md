# Tesseract
Source code for the chess engine used in the thesis. Developed with Visual Studio C++ and requires C++20 with BMI2 support for the relevant PEXT/PDEP instructions.

Relevant settings in Visual Studio to get maximum performance:
- Optimization: "Maximum Optimization (Favor Speed) (/O2)"
- Favor Size Or Speed: "Favor fast code (/Ot)"
- Enable Incremental Linking: "No (/INCREMENTAL:NO)"
- Link Time Code Generation: "Use Link Time Code Generation (/LTCG)"
