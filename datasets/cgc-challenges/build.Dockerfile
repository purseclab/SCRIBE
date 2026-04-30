FROM ubuntu:focal AS cgc-build

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_REPO=https://github.com/novafacing/cgc-challenges
ARG GIT_COMMIT=44bb316dfadddd94d70a45cd0d7eaadd43138647
ARG CFLAGS
ARG CXXFLAGS

RUN dpkg --add-architecture i386
RUN apt-get update && apt-get install -y \
    git python3 python3-pip python2 build-essential && \
    rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install meson ninja pwntools

RUN git clone $GIT_REPO && \
    mkdir -p /output && \
    cd `basename -s ".git" $GIT_REPO` && \
    git checkout $GIT_COMMIT && \
    meson setup \
    -Dc_args="$CFLAGS" -Dinstall_path=/output -Dinstall_pip_modules=true \
    "-Ddisabled=['3D_Image_Toolkit', 'AIS-Lite', 'ASCII_Content_Server', 'ASL6parse', 'Accel', 'A_Game_of_Chance', 'Audio_Visualizer', 'Azurad', 'BIRC', 'Barcoder', 'Bloomy_Sunday', 'Blubber', 'CGC_Board', 'CGC_Hangman_Game', 'CGC_Image_Parser', 'CGC_Symbol_Viewer_CSV', 'CML', 'CNMP', 'CLOUDCOMPUTE', 'Carbonate', 'Cereal_Mixup__A_Cereal_Vending_Machine_Controller', 'Charter', 'Childs_Game', 'Corinth', 'DFARS_Sample_Service', 'Diary_Parser', 'Differ', 'Dive_Logger', 'Document_Rendering_Engine', 'Dungeon_Master', 'Enslavednode_chat', 'Estadio', 'EternalPass', 'FASTLANE', 'FISHYXML', 'FSK_BBS', 'FUN', 'FaceMag', 'FailAV', 'FileSys', 'Filesystem_Command_Shell', 'Finicky_File_Folder', 'Fortress', 'GREYMATTER', 'Glue', 'Griswold', 'H20FlowInc', 'HIGHCOO', 'Headscratch', 'HeartThrob', 'Hug_Game', 'Image_Compressor', 'KKVS', 'KTY_Pretty_Printer', 'Kaprica_Script_Interpreter', 'LAN_Simulator', 'LazyCalc', 'Lazybox', 'LulzChat', 'Material_Temperature_Simulation', 'Matrix_Math_Calculator', 'Messaging', 'Mixology', 'Mount_Filemore', 'Movie_Rental_Service', 'Movie_Rental_Service_Redux', 'Multi_Arena_Pursuit_Simulator', 'Multi_User_Calendar', 'Multicast_Chat_Server', 'Multipass', 'Multipass2', 'Multipass3', 'Music_Store_Client', 'NarfAgainShell', 'Network_File_System', 'Network_File_System_v3', 'Network_Queuing_Simulator', 'No_Paper._Not_Ever._NOPE', 'NoHiC', 'OTPSim', 'OUTLAW', 'OUTLAW_3', 'On_Sale', 'One_Amp', 'Order_Up', 'PCM_Message_decoder', 'PKK_Steganography', 'PTaaS', 'Pac_for_Edges', 'Packet_Receiver', 'Pattern_Finder', 'Pipelined', 'Printer', 'QUIETSQUARE', 'QuadtreeConways', 'Query_Calculator', 'RAM_based_filesystem', 'REDPILL', 'REMATCH_1--Hat_Trick--Morris_Worm', 'REMATCH_2--Mail_Server--Crackaddr', 'REMATCH_3--Address_Resolution_Service--SQL_Slammer', 'REMATCH_4--CGCRPC_Server--MS08-067', 'REMATCH_5--File_Explorer--LNK_Bug', 'REMATCH_6--Secure_Server--Heartbleed', 'RRPN', 'Recipe_and_Pantry_Manager', 'Rejistar', 'Resort_Modeller', 'SAuth', 'SCUBA_Dive_Logging', 'SIGSEGV', 'SLUR_reference_implementation', 'SOLFEDGE', 'SPIFFS', 'Sad_Face_Template_Engine_SFTE', 'Scrum_Database', 'Secure_Compression', 'Sensr', 'Shipgame', 'Shortest_Path_Tree_Calculator', 'ShoutCTF', 'Simple_Stack_Machine', 'Single-Sign-On', 'Sorter', 'Space_Attackers', 'Square_Rabbit', 'Stock_Exchange_Simulator', 'Street_map_service', 'String_Info_Calculator', 'String_Storage_and_Retrieval', 'TFTTP', 'TIACA', 'TVS', 'Tennis_Ball_Motion_Calculator', 'Terrible_Ticket_Tracker', 'TextSearch', 'The_Longest_Road', 'Thermal_Controller_v2', 'Vector_Graphics_2', 'Vector_Graphics_Format', 'Venture_Calculator', 'Virtual_Machine', 'Water_Treatment_Facility_Simulator', 'XStore', 'anagram_game', 'commerce_webscale', 'electronictrading', 'greeter', 'humaninterface', 'matrices_for_sale', 'middleout', 'middleware_handshake', 'netstorage', 'online_job_application', 'online_job_application2', 'payroll', 'pizza_ordering_system', 'reallystream', 'router_simulator', 'simpleOCR', 'simplenote', 'stack_vm', 'stream_vm2', 'tribute', 'university_enrollment', 'vFilter', 'virtual_pet', 'yolodex', 'Casino_Games', 'Eddy', 'Monster_Game', 'UTF-late', 'WhackJack', 'chess_mimic', 'Message_Service', 'CGC_Planet_Markup_Language_Parser', 'Particle_Simulator', 'CGC_Video_Format_Parser_and_Viewer', 'CGC_File_System', 'Snail_Mail', 'basic_messaging', 'One_Vote', 'Kaprica_Go', 'WordCompletion', 'hawaii_sets', 'CableGrind']" \
    builddir && \
    cd builddir && \
    meson compile && \
    meson install

# strip binaries
RUN cp -r /output/bin /output/bin-unstripped
RUN if [ -n "$TARGET" ]; then export STRIP=$TARGET-strip; else export STRIP=strip; fi && \
    echo "Using strip command: $STRIP" && \
    find /output/bin -type f -exec $STRIP {} \;

WORKDIR cgc-challenges/builddir
