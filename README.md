sequenceDiagram
    participant Shell
    participant Main as Main()
    participant Source File
    participant queue
    participant HasSpace as std::condition_variable HasSpace
    participant HasData as std::condition_variable hasData

    activate Shell
    Shell->>Main: Source and destination path
    activate Main
    alt files can be open
        deactivate Shell
        activate Source File
        Main-->>Source File: open´
        Create Participant Destination as Destionation File
        Main-->>+Destination: open
        Create participant Reader as Reader(file, queue)
        Main-->>+Reader: std::ifstream, queue
        Create Participant Writer as Writer(file, queue)
        Main-->>+Writer: std::ofstream, queue
        deactivate Main
        loop until copy finished
            par
                alt has free space
                    Reader-->>queue: chunk
                    Reader-->>HasData: notify
                else
                    Reader -->> HasSpace: wait
                end
            and
                alt has data
                    queue-->>Writer: chunk
                    Writer-->>HasSpace: notify
                else
                    Writer-->>HasData: wait
                end
            end
        end
        Writer-->Main: (Finished)
        activate Main
        Deactivate Writer
        Main-->>Reader: Join
        deactivate Main
        Reader-->>Main: Joined
        activate Main
        Deactivate Reader
        Main->>+Shell: Result code: 0;
        Deactivate Main
        Deactivate Source File
        Deactivate Destination
    else error openning a file
        Main ->>+ Shell : Error messsage, error code
    end
