extern int cmd_index;
extern char cmd_buffer[256];
extern char current_color;

// Ārējās funkcijas, ko uzrakstīji kernelī
extern void print(const char *str);
extern void clear_screen();

// Mūsu strcmp funkcija
int strcmp(const char *str1, const char *str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) return 1;
        i++;
    }
    if (str1[i] == '\0' && str2[i] == '\0') return 0;
    return 1;
}


void execute_command(void){
    if (cmd_index == 0) {
        print("\n> "); // Ja nekas nav uzrakstīts, vienkārši jauna rinda
        return;
    }

    cmd_buffer[cmd_index] = '\0'; // Nobeidzam teksta virkni ar drošības nulli

    if(strcmp(cmd_buffer, "clear") == 0){
         clear_screen();
         current_color=0x0A;
         print("> ");
   }else if(strcmp(cmd_buffer, "about") == 0) {
         print("\n");
         print("        <--Par manu OS!-->\n");
         print("\n");
         print("Si kernel izstradatajs ir Kristaps\n");
         print("Sis ir 64bitu kernelis\n");
         print("Raksti main, lai atgriezots sakuma\n");
         print("> ");

      }else if(strcmp(cmd_buffer, "main") == 0) {
           clear_screen();

    current_color = 0x0E;
    print("=== KristapsOS kernelis v0.0.4 ===\n");

    current_color = 0x0A;
    print("Startejam kernel ");
    init_idt();
    print("[ GATAVS ]\n\n");

    print("Sistema gaida ievadu\n> ");

    // Uzstādām glītu zilo joslu apakšā
    current_color = 0x1F;
    for (int x = 0; x < VGA_WIDTH; x++) put_char_at(' ', current_color, x, 24);
    const char *status = " Statusi: [ IDT: Aktivs ]   [ Tastatura: GAIDA IEVADI ]";
    int s = 0;
    while (status[s] != '\0') {
        put_char_at(status[s], current_color, s + 2, 24);
        s++;
    }

    // Atgriežam krāsu zaļu priekš lietotāja rakstītā teksta
    current_color = 0x0A;
}else if(strcmp(cmd_buffer, "version") == 0){
           print("\n");
           print("      |-------------------------|\n");
           print("      |Kernela versija ir v0.0.4|\n");
           print("      |-------------------------|\n");
           print("> ");

        }else if(strcmp(cmd_buffer, "help") == 0){
          print("\n");
          print("Pieejamas komandas:\n");
          print("1. help\n");
          print("2. clear\n");
          print("3. version\n");
          print("4. about\n");
          print("5. cpu\n");
          print("6. color\n");
          print("7. main\n");
          print("\n");
          print("> ");

        }



        else{
         print("\nKomanda \""); // Izvada sākumu (un smukas pēdiņas)
        print(cmd_buffer);     // Izvada pašu nepareizo vārdu no atmiņas
        print("\" nav atpazita!\n "); // Izvada beigas un jaunu uzaicinājuma rindu
         print("> ");
}

cmd_index=0;
}
