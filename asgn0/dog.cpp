#include <unistd.h>
#include <iostream>
#include <string.h>
#include <fcntl.h>
#include <err.h>

void stdInRead()
{
    int eof = 32767;
    while(eof != 0)
    {
        char buff[32768] = "";
        eof = read(0, buff, eof);
        //cout << eof << endl;

        if(eof == 0)
        {
            //cout << "I found the end of file" << endl;
            break;
        }
        else
        {
            write(1, buff, eof);
        }
    }
}

int main(int argc, char* argv[])
{
    using namespace std;
    //cout << argc << " " << argv[1] << endl;
    if(argc == 1) stdInRead();

    for(int i = 1; i < argc; i++)
    {
        if(0 == strcmp("-", argv[i]))
        {
            stdInRead();
        }

        else
        {
            int fd = open(argv[i], 0);
            char isDirectory[1] = "";
            
            //cout << fd << endl;
            if(fd == -1)
            {
                warn("%s", argv[i]);
            }

            else if(read(fd, isDirectory, 0) == -1)
            {
                warn("%s", argv[i]);
            }
            else
            {
                int eof = 32767;
                while(eof != 0)
                {
                    char buff[32768] = "";
                    eof = read(fd, buff, eof);
                    cout << eof << endl;
                    write(1, buff, eof);
                }
            }
            close(fd);
        }
    }
}
