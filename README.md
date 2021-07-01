# Ntuple Analysis
This is a suite of programs and scripts for histogramming and reweighting of GoSam and BlackHat ntuples.

For the details of the structure of the ntuples, please refer to:
- [arXiv:1310.7439](https://arxiv.org/abs/1310.7439),
- [arXiv:1608.01195](https://arxiv.org/abs/1608.01195),
- [CERN-THESIS-2021-047](https://cds.cern.ch/record/2766569) Appendix A.

This readme is a work in progress.
I will continue adding to it to make it a comprehensive guide to working with GoSam ntuples.

# Quick start
## Checking out the code
```
git clone --recursive https://github.com/ivankp/ntuple_analysis.git
```

Don't forget the `--recursive` argument to also clone the required submodules.

## How to run
1. Edit `src/hist.cc`. This is the program that reads the ntuples and fills the defined histograms.
It also does reweighting.
2. Add histograms' definitions
[around line 384](https://github.com/ivankp/ntuple_analysis/blob/master/src/hist.cc#L384).
Basically, add `h_(name_of_observable)`.
3. Fill histograms towards the end of the event loop,
[around line 553](https://github.com/ivankp/ntuple_analysis/blob/master/src/hist.cc#L553).
4. Define histograms' binning in `run/binning.json`.
5. Select what sets of ntuples to run over in `run/submit.sh`.
6. Generate job scripts and submit them to condor by running `./submit.sh` inside the `run` directory.

# Detailed guide
## Software requirements
### C++ compiler
A `C++` compiler supporting `C++20` features is required.
For `GCC` this means version `10.1.0` or higher.

### CERN ROOT
Get at: https://root.cern/

A recent enough version of `ROOT` that allows compilation with the `-std=c++20` flag is reguired.
The programs have been tested with version `6.18/04` and `6.20/04`.
Any newer version should work. Older versions are most likely ok as well, as long as the code compiles.

### LHAPDF
Get at: https://lhapdf.hepforge.org/

`LHAPDF` is required to compute scale variations and PDF uncertainties.

### FastJet
Get at: http://fastjet.fr/

`FastJet` is required to cluster final state particles into jets.

## Installation
- Recommend cloning the repo

## Running an analysis
### Defining histograms

### Defining binning

### Defining runcards

### Running the histogramming program

### Running on Condor

## Implementation details
### Histogramming library

### Containers library

## branch_reader

### JSON parser

# Advanced topics
## Making use of additional branches

## Changing bin types and tags

## Changing output format

## Adding dependencies in Makefile
