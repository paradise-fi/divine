int main()
{
    double x = 1e300;
    float y = x;
    if ( y ) /* ERROR */
        return 0;
    return 1;
}
