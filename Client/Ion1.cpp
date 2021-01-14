#include <fstream>

using namespace std;
ifstream fin("pb1.in");
ofstream fout("pb1.out");

int a, b;
int main()
{
    fin >> a >> b;
    fout << a + b;
    return 0;
}
