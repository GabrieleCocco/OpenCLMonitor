// TestExe.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <Windows.h>
using namespace std;

int _tmain(int argc, _TCHAR* argv[])
{
  ofstream myfile;
  myfile.open ("C:\\Users\\gabriele\\OpenCLSpy\\SAMPLE.txt");
  myfile << "Writing this to a file.\n";
  myfile.close();
  Sleep(10000);
  DeleteFile(L"C:\\Users\\gabriele\\OpenCLSpy\\SAMPLE.txt");
  return 0;
}

