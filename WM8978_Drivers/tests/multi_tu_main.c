int wm8978_multi_tu_a(void);
int wm8978_multi_tu_b(void);

int main(void)
{
    return (wm8978_multi_tu_a() == 1) ? wm8978_multi_tu_b() : 1;
}
