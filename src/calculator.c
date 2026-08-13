extern char input[];
extern void print(const char* str);
extern void put_char(char c);
extern void update_cursor(void);

extern void input_text(const char* prompt, char* buffer, int buffer_size);

int string_to_int(const char* str) {
    int num = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        num = num * 10 + (*str - '0');
        str++;
    }
    return num * sign;
}

void print_int(int n) {
    if (n == 0) {
        put_char('0');
        return;
    }

    if (n < 0) {
        put_char('-');
        n = -n;
    }

    char buf[12];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    while (--i >= 0) {
        put_char(buf[i]);
    }
}


int add(int a, int b) {
    return a + b;
}

int sub(int a, int b) {
    return a - b;
}

int mul(int a, int b) {
    return a * b;
}

int div(int a, int b) {
    return a / b;
}

int pow(int a, int b) {
    int result = 1;
    for (int i = 0; i < b; i++) {
        result = result * a;
    }
    return result;
}

int rmn(int a, int b) {
    while (a >= b) {
        a = a - b;
    }

    if (a == 0) {
        return 0;
    } else {
        return b - a;
    }

}

void calculator(void) {
    print("=== Welcome to Q-J-R OS calculator v1.1 ===\n");

    char expression[64];

    input_text(
        "Enter expression (for example: 10+5) -> ",
        expression,
        sizeof(expression)
    );

    char* first_num_str = expression;
    int i = 0;

    while (expression[i] != '+' &&
           expression[i] != '-' &&
           expression[i] != '*' &&
           expression[i] != '/' &&
           expression[i] != '%' &&
           expression[i] != '\0') {
        i++;
    }

    if (expression[i] == '+') {
        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = add(a, b);

        print("Result > ");
        print_int(result);
        print("\n");

    } else if (expression[i] == '-') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = sub(a, b);

        print("Result > ");
        print_int(result);
        print("\n");


    } else if (expression[i] == '*') {
        int a = string_to_int(first_num_str);

        if (expression[i + 1] == '*') {
            int b = string_to_int(&expression[i + 2]);

            int result = pow(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        } else {
            int b = string_to_int(&expression[i + 1]);

            int result = mul(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        }

    } else if (expression[i] == '/') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        if (b == 0) {
            print("Result > Infinity\n");
        } else {
            int result = div(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        }
    } else if (expression[i] == '%') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = rmn(a, b);

        print("Result > ");
        print_int(result);
        print("\n");



    } else {
        print("Error: Please use these operators:\n");
        print(" +  -> Addition\n");
        print(" -  -> Subtraction\n");
        print(" *  -> Multiplication\n");
        print(" /  -> Division\n");
        print(" %  -> Remainder\n");
        print(" ** -> Power\n");
        print("Usage: <number1><operator><number2>\n");
    }
}

// calculator API
void calc(char* expression) {

    char* first_num_str = expression;
    int i = 0;

    while (expression[i] != '+' &&
           expression[i] != '-' &&
           expression[i] != '*' &&
           expression[i] != '/' &&
           expression[i] != '%' &&
           expression[i] != '\0') {
        i++;
    }

    if (expression[i] == '+') {
        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = add(a, b);

        print("Result > ");
        print_int(result);
        print("\n");

    } else if (expression[i] == '-') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = sub(a, b);

        print("Result > ");
        print_int(result);
        print("\n");


    } else if (expression[i] == '*') {
        int a = string_to_int(first_num_str);

        if (expression[i + 1] == '*') {
            int b = string_to_int(&expression[i + 2]);

            int result = pow(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        } else {
            int b = string_to_int(&expression[i + 1]);

            int result = mul(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        }

    } else if (expression[i] == '/') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        if (b == 0) {
            print("Result > Infinity\n");
        } else {
            int result = div(a, b);

            print("Result > ");
            print_int(result);
            print("\n");
        }
    } else if (expression[i] == '%') {

        int a = string_to_int(first_num_str);
        int b = string_to_int(&expression[i + 1]);

        int result = rmn(a, b);

        print("Result > ");
        print_int(result);
        print("\n");


    } else {
        print("Error: Please use these operators:\n");
        print(" +  -> Addition\n");
        print(" -  -> Subtraction\n");
        print(" *  -> Multiplication\n");
        print(" /  -> Division\n");
        print(" %  -> Remainder\n");
        print(" ** -> Power\n");
        print("Usage: <number1><operator><number2>\n");
    }
}