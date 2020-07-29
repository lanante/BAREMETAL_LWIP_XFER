if exist('u')
    fclose (u);
    delete(u);
    clear u;
end
clear
clc
global XFER_CMD XFER_PAR XFER_STATE XFER_DEVICE_STATE
XFER_CMD = struct('ST', 0, 'DATA', 1, 'BAR', 2,'BA', 3, 'REQ', 4, 'END', 5, 'STATE', 6, 'TX_ST', 7,'RX_END', 7);
XFER_PAR = struct('MAX_BURST', 2^9, 'SEGMENT_SIZE',2^12,'MAX_SEGMENTS',2^16);  %SEGMENT_SIZE>2^12 is received twice so 2^12 is max
XFER_STATE = struct('IDLE', 0, 'DATA',1,'TX_ST',2);
XFER_DEVICE_STATE = struct('IDLE', 0, 'TX',1,'RX',2);


u=udp;
u.LocalPort=5002;
u.RemoteHost='192.168.1.10';
u.RemotePort=5001;

u.InputBufferSize=XFER_PAR.SEGMENT_SIZE*XFER_PAR.MAX_BURST*2;
u.OutputBufferSize=XFER_PAR.SEGMENT_SIZE*2;
fopen(u);



TOTAL_TX_SIZE=2^20; %Send 2^20 octets
NUM_SEGS=ceil(TOTAL_TX_SIZE/XFER_PAR.SEGMENT_SIZE);
TX_DATA=dec2hex(randi(2^8,NUM_SEGS*XFER_PAR.SEGMENT_SIZE,1)-1);

if NUM_SEGS>=XFER_PAR.MAX_SEGMENTS
    error('Exceeded Max number of segments');
end


state=0;
next_state=0;
TX=1;
%First send a STATE packet to register as master
flushinput(u);
udp_xfer_send(u,XFER_CMD.STATE, 0,0,0,0,XFER_DEVICE_STATE.RX);
payload=fread(u,1,'uint8');
if (payload~=XFER_CMD.STATE)
    error('Cannot connect to device');
end
disp('Connected to device');
        disp('Starting TX');


while (1)
    if TX
        switch  state
            case XFER_STATE.IDLE %IDLE
                udp_xfer_send(u,XFER_CMD.ST, NUM_SEGS,0,0,0,0);
                BITMAP=zeros(1,2^16);
                payload=fread(u,1,'uint8');
                if payload(1)==XFER_CMD.ST
                    next_state=XFER_STATE.DATA;
                    tic
                else
                    next_state=XFER_STATE.IDLE;
                end
                
            case XFER_STATE.DATA
                %SEND DATA
                tx_fragment_list=find(BITMAP(1:NUM_SEGS)==0);
                while (1)
                    for i=1:length(tx_fragment_list)
                        SEG_NUM=tx_fragment_list(i);
                        DATA=TX_DATA((SEG_NUM-1)*XFER_PAR.SEGMENT_SIZE+1:SEG_NUM*XFER_PAR.SEGMENT_SIZE,:);
                        udp_xfer_send(u,XFER_CMD.DATA,0,DATA,SEG_NUM-1,0,0);
                    end
                    
                    %SEND BAR
                    flushinput(u);
                    udp_xfer_send(u,XFER_CMD.BAR,0,0,0,0,0);
                    payload=fread(u,1,'uint8');
                    if payload(1)==XFER_CMD.BA
                        BITMAP_BYTE=payload(5:end);
                        BITMAP=reshape(de2bi(BITMAP_BYTE).',1,[]);
                        tx_fragment_list=find(BITMAP(1:NUM_SEGS)==0);
                        if isempty(tx_fragment_list)
                            next_state=XFER_STATE.TX_ST;
                            TX_XFER_TIME=toc;
                            break;
                        end
                    else
                        error('BA was not returned');
                    end
                end
                
            case XFER_STATE.TX_ST %INITIATE TX
                udp_xfer_send(u,XFER_CMD.TX_ST,0,0,0,0,XFER_DEVICE_STATE.RX);
                payload=fread(u,1,'uint8');
                if payload(1)==XFER_CMD.TX_ST
                    TX=0;
                end
                next_state=XFER_STATE.IDLE;
                disp(['DL rate is ' num2str(TOTAL_TX_SIZE/(TX_XFER_TIME)*8/1e6) ' Mbps'])
                flushinput(u);
        end
    else
        switch  state
            case XFER_STATE.IDLE %IDLE
                flushinput(u); %Change deice stat to RX
                udp_xfer_send(u,XFER_CMD.STATE, 0,0,0,0,XFER_DEVICE_STATE.RX);
                payload=fread(u,1,'uint8');
                if (payload~=XFER_CMD.STATE)
                    error('Cannot connect to device');
                end
                
                while(1)  %Wait for XFER_CMD.ST from device
                    if u.BytesAvailable>=4
                        payload=fread(u,4,'uint8');
                        if payload(1)==XFER_CMD.ST
                            NUM_SEGS=bi2de([de2bi(payload(3)) de2bi(payload(4))]);
                            BITMAP=zeros(1,8*ceil(NUM_SEGS/8)); %Total Number of SEGMENTS TO REQUEST
                            next_state=XFER_STATE.DATA;
                            TOTAL_RX_SIZE=NUM_SEGS*XFER_PAR.SEGMENT_SIZE;
                            tic
                            break;
                        else
                            next_state=XFER_STATE.IDLE;
                            flushinput(u);
                        end
                    end
                end
            case XFER_STATE.DATA
                payload=zeros(1,NUM_SEGS*XFER_PAR.SEGMENT_SIZE/4);
                while (sum(BITMAP)<NUM_SEGS)
                    rx_fragment_list=find(BITMAP==0);
                    BURST_NUM=min(XFER_PAR.MAX_BURST,NUM_SEGS-sum(BITMAP));
                    REQ_INDEX=rx_fragment_list(1:BURST_NUM);
                    REQUESTED_BITMAP=BITMAP*0;
                    REQUESTED_BITMAP( REQ_INDEX)=1;
                    udp_xfer_send(u,XFER_CMD.REQ,NUM_SEGS,0,0,REQUESTED_BITMAP,0);
                    SEGS=0;
                    while(1)
                        if (u.BytesAvailable>=(XFER_PAR.SEGMENT_SIZE+4))
                            temp=flipud(dec2hex(fread(u,XFER_PAR.SEGMENT_SIZE+4,'uint8')).');
                            SEG_NUM=hex2dec(fliplr([temp(:,3); temp(:,4)].'))+1;
                            temp2=hex2dec(fliplr(reshape(temp,8,[]).'));
                            payload((SEG_NUM-1)*XFER_PAR.SEGMENT_SIZE/4+1:SEG_NUM*XFER_PAR.SEGMENT_SIZE/4)=temp2(2:end);
                            BITMAP(SEG_NUM)=1;
                            SEGS=SEGS+1;
                        elseif (SEGS>=BURST_NUM )
                            flushinput(u);
                            break;
                        end
                    end
                    
                end
                RX_XFER_TIME=toc;
                next_state=XFER_STATE.IDLE;
disp(['UL rate is ' num2str(TOTAL_RX_SIZE/(RX_XFER_TIME)*8/1e6) ' Mbps'])
break;
        end
    end
    state=next_state;
    
end
disp('Transfer Ended');



function udp_xfer_send(u,CMD, NUM_SEGS,DATA,SEG_NUM,BITMAP,DEVICE_STATE)
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
        
    case {XFER_CMD.BAR,XFER_CMD.TX_ST}
        PAYLOAD=[dec2hex(CMD,2);PAD;PAD;PAD];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
    case XFER_CMD.REQ
        BITMAPBYTE=dec2hex(bi2de(reshape(BITMAP,8,[]).'),2);
        NUM_SEGS_HEX=dec2hex(NUM_SEGS,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;NUM_SEGS_HEX(3:4);NUM_SEGS_HEX(1:2);BITMAPBYTE];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
    case XFER_CMD.STATE
        STATE=dec2hex(DEVICE_STATE,2);
        PAYLOAD=[dec2hex(CMD,2);PAD;PAD;STATE];
        fwrite(u,hex2dec(PAYLOAD),'uint8');
end

end




