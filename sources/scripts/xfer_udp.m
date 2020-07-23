if exist('u')
    fclose (u);
    delete(u);
    clear u;
end
clear
clc
global XFER_CMD XFER_PAR
XFER_CMD = struct('ST', 0, 'DATA', 1, 'BAR', 2,'BA', 3, 'REQ', 4, 'END', 5);
XFER_PAR = struct('MAX_BURST', 2^9, 'SEGMENT_SIZE',2^13,'MAX_SEGMENTS',2^16);


u=udp;
u.LocalPort=7;
u.RemoteHost='192.168.1.10';
u.RemotePort=5001;

u.InputBufferSize=XFER_PAR.SEGMENT_SIZE*XFER_PAR.MAX_BURST*2;
u.OutputBufferSize=XFER_PAR.SEGMENT_SIZE*2;
fopen(u);



TOTAL_TX_SIZE=2^20; %Send 2^20 octets
NUM_SEGS=ceil(TOTAL_TX_SIZE/XFER_PAR.SEGMENT_SIZE);
PACKET=dec2hex(randi(2^8,NUM_SEGS*XFER_PAR.SEGMENT_SIZE,1)-1);
if NUM_SEGS>=XFER_PAR.MAX_SEGMENTS
    error('Exceeded Max number of segments');
end


state=0;
next_state=0;
TX=0;
while (1)
    if TX
        switch  state
            case 0 %IDLE
                SEND_PACKET(u,XFER_CMD.ST, NUM_SEGS,0,0,0);
                BITMAP=zeros(1,2^16);
                payload=fread(u,1,'uint8');
                if payload(1)==XFER_ST
                    next_state=1;
                else
                    next_state=0;
                end
                
            case 1
                %SEND DATA
                tx_fragment_list=find(BITMAP(1:NUM_SEGS)==0);
                for i=1:length(tx_fragment_list)
                    SEG_NUM=tx_fragment_list(i);
                    DATA=PACKET((SEG_NUM-1)*XFER_PAR.SEGMENT_SIZE+1:SEG_NUM*XFER_PAR.SEGMENT_SIZE,:);
                    SEND_PACKET(u,XFER_CMD.DATA,0,DATA,SEG_NUM-1);
                end
                
                %SEND BAR
                SEND_PACKET(u,XFER_CMD.BAR,0,0,0);
                payload=fread(u,1,'uint8');
                if payload(1)==XFER_CMD.BA
                    BITMAP_BYTE=payload(5:end);
                    BITMAP=reshape(de2bi(BITMAP_BYTE).',1,[]);
                    if sum(BITMAP)==NUM_SEGS
                        next_state=2;
                    end
                else
                    next_state=0;
                    
                end
            case 2 %INITIATE TX
                next_state=3;
                TX=0;
                break;
        end
    else
        switch  state
            case 0 %IDLE
                flushinput(u);
                while(1)  %Wait for XFER_CMD.ST from device
                    if u.BytesAvailable>=4
                        payload=fread(u,4,'uint8');
                        if payload(1)==XFER_CMD.ST
                            NUM_SEGS=bi2de([de2bi(payload(3)) de2bi(payload(4))]);
                            BITMAP=zeros(1,8*ceil(NUM_SEGS/8)); %Total Number of SEGMENTS TO REQUEST
                            next_state=1;
                            break;
                        else
                            flushinput(u);
                        end
                    end
                end
            case 1
                REQUESTED_BITMAP=BITMAP;
                payload=zeros(1,NUM_SEGS*XFER_PAR.SEGMENT_SIZE/4);
                while (sum(BITMAP)<NUM_SEGS)
                    rx_fragment_list=find(BITMAP==0);
                    BURST_NUM=min(MAX_BURST,NUM_SEGS-sum(BITMAP));
                    REQ_INDEX=rx_fragment_list(1:BURST_NUM);
                    REQUESTED_BITMAP( REQ_INDEX)=1;
                    SEND_PACKET(u,XFER_CMD.REQ,NUM_SEGS,0,0,REQUESTED_BITMAP);
                    SEGS=0;
                    while((u.BytesAvailable>=(XFER_PAR.SEGMENT_SIZE+4))&&SEGS<BURST_NUM )
                        temp=flipud(dec2hex(fread(u,XFER_PAR.SEGMENT_SIZE+4,'uint8')).');
                        SEG_NUM=hex2dec(fliplr([temp(:,3); temp(:,4)].'))+1;
                        temp2=hex2dec(fliplr(reshape(temp,8,[]).'));
                        payload((SEG_NUM-1)*XFER_PAR.SEGMENT_SIZE/8+1:SEG_NUM*XFER_PAR.SEGMENT_SIZE/8)=temp2(2:end);
                        BITMAP(SEG_NUM)=1;
                        SEGS=SEGS+1;
                    end
                    
                end
                next_state=0;
        end
        state=next_state;
        
    end
    
end



function SEND_PACKET(u,CMD, NUM_SEGS,DATA,SEG_NUM,BITMAP)
global XFER_CMD

PAD=dec2hex(0,2);
switch CMD
    case XFER_CMD.ST
        NUM_SEGS_HEX=dec2hex(NUM_SEGS,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;NUM_SEGS_HEX(3:4);NUM_SEGS_HEX(1:2)];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
        
    case XFER_CMD.DATA
        SEG_NUM_HEX=dec2hex(SEG_NUM,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;SEG_NUM_HEX(3:4);SEG_NUM_HEX(1:2);DATA];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
        
    case XFER_CMD.BAR
        PAYLOAD=[dec2hex(CMD,2);PAD;PAD;PAD];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
    case XFER_CMD.REQ
        BITMAPBYTE=dec2hex(bi2de(reshape(BITMAP,8,[]).'),2);
        NUM_SEGS_HEX=dec2hex(NUM_SEGS,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;NUM_SEGS_HEX(3:4);NUM_SEGS_HEX(1:2);BITMAPBYTE];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
        
end

end




