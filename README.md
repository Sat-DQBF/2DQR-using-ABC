# 2DQR ABC

This is an implementation of 2DQR with ABC as the PDR solver

## Building this project
- Install [`vcpkg`](https://github.com/microsoft/vcpkg)
- Install `cxxopts` and `boost-process` with `{VCPKG_ROOT}/vcpkg install cxxopts boost-process`
- Run the following commands

```
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE={VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake ..
make
```
- (MacOS) Please install [`homebrew`](https://brew.sh) and run `brew install coreutils`

## Usage

```2DQR
Usage:
  2dqr [OPTION...]

  -i, --input arg    Input File
  -s, --skolem       Generate Skolem functions
      --no_fraig     Disable optimising Skolem functions with fraig
      --abc_bin arg  ABC binary path (default: ../abc/abc)
  -v, --verbose      Verbose output
  -h, --help         Print usage
```

# Extended format for DQCIR

The extended format is divided into three parts: 

## Header

The header starts with the line `#QCIR-14` and contains the information about the variables, each line is of the form 

- `forall(<vars>)`
- `exists(<vars>)`
- `depend(<var>, <vars>)`
- `output(<var>)`

where `<var>` is a single variable and `<vars>` is a sequence of variables separated with `,`.

## Circuit

Each line in this section either starts with `#` or is a gate of the form `<var> = <opr>(<lits>)`, where `<opr>` is one of `and` and `or`, and `<lits>` is a sequence of variables and negated variables (with prefix of `-`) separated with `,`.

The circuit section is divided into n + 2 parts, the first part is the common circuit, which each gate does not depend on any existential variables. 

The next n parts starts with the line `#! <var1> <var2>` where `<var1>` and `<var2>` are distinct existential variables, followed with a circuit which each gate only depends on the universal variables or the gates in the common circuit or `<var1>` or `<var2>`.

The last part starts with the line `#! end` and followed by one and gate with the name being the output variable, and the input of that gate are the last gate of each of the n parts above.