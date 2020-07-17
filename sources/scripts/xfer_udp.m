if exist('u')
    fclose (u);
    delete(u);
    clear u;
end
clear
clc

u=udp;
u.LocalPort=7;
u.RemoteHost='192.168.1.10';
u.RemotePort=5001;

u.InputBufferSize=16384;
fopen(u);
XFER_ST=0;
XFER_DATA=1;
XFER_BAR=2;
XFER_BA=3;

PACKET_SIZE=2^20; %Send 2^20 octets
FRAG_SIZE=1472; %octets per fragment
NUM_SEGS=ceil(PACKET_SIZE/FRAG_SIZE);
PACKET=dec2hex(randi(2^8,NUM_SEGS*FRAG_SIZE,1)-1);
if NUM_SEGS>=2^16
    error('Exceeded Max number of fragments');
end
    
    
state=0;
next_state=0;
TX=1;
while (1)
    switch  state
        case 0 %IDLE
            if TX
                SEND_PACKET(u,XFER_ST, NUM_SEGS,0,0,0);
                BITMAP=zeros(1,2^16);
                payload=fread(u,1,'uint8');
                if payload(1)==XFER_ST
                    next_state=1;
                else
                    next_state=0;
                end
            else
                next_state=0;
            end
        case 1
            %SEND DATA
            tx_fragment_list=find(BITMAP(1:NUM_SEGS)==0);
            for i=1:length(tx_fragment_list)
                SEG_NUM=tx_fragment_list(i);
                DATA=PACKET((SEG_NUM-1)*FRAG_SIZE+1:SEG_NUM*FRAG_SIZE,:);
                SEND_PACKET(u,XFER_DATA,0,DATA,SEG_NUM-1);
            end
            
            %SEND BAR
            SEND_PACKET(u,XFER_BAR,0,0,0);
            payload=fread(u,1,'uint8');
            if payload(1)==XFER_BA
                BITMAP_BYTE=payload(5:end);
                BITMAP=reshape(de2bi(BITMAP_BYTE).',1,[]);
                if sum(BITMAP)==NUM_SEGS
                    next_state=2;
                end
            else
                next_state=0;
                
            end
        case 2 %END
            break;
    end
    state=next_state;
end
disp('Send Finished');

function SEND_PACKET(u,CMD, NUM_SEGS,DATA,SEG_NUM,BITMAP)
XFER_ST=0;
XFER_DATA=1;
XFER_BAR=2;
XFER_BA=3;
PAD=dec2hex(0,2);
switch CMD
    case XFER_ST
        NUM_SEGS_HEX=dec2hex(NUM_SEGS,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;NUM_SEGS_HEX(3:4);NUM_SEGS_HEX(1:2)];
                 fwrite(u,hex2dec(PAYLOAD),'uint8');

    case XFER_DATA
                SEG_NUM_HEX=dec2hex(SEG_NUM,4);
        PAYLOAD=[dec2hex(CMD,2);PAD;SEG_NUM_HEX(3:4);SEG_NUM_HEX(1:2);DATA];
                 fwrite(u,hex2dec(PAYLOAD(1:100,:)),'uint8');

    case XFER_BAR
        PAYLOAD=[dec2hex(CMD,2);PAD;PAD;PAD];
                         fwrite(u,hex2dec(PAYLOAD),'uint8');

end

end

%
% DATA=dec2hex(randi(2^8,FRAG_SIZE,1)-1);
% % txdata = [CMD;SEG_NUM(3:4);SEG_NUM(1:2);'04';'05';'06';'07'];
% fwrite(u,hex2dec(txdata),'uint8')
%
%
%
%
% %
% fwrite(u,hex2dec(txdata),'uint8')

















% T_Window=5;
% t_step=0.01;
% t=t_step:t_step:T_Window;
% start=0;
% prev=0;
% MODE=0;
% PHY_SAMP=122.88e6;
% while 1
%     if u.BytesAvailable>=(5+256)*4
%         AT=dec2bin(fread(u,1,'uint32'));
%     end
%     if size(AT,1)>=(5+256)*1
%         A=AT(1:1:end,:);
%         B=([A(:,25:32) A(:,17:24) A(:,9:16) A(:,1:8)]); %
%         TIME_STAMP=bin2dec(B(1,:));
%         L_SIG=fliplr(B(2,end-24+1:end));
%         RX_FORMAT=bin2dec(B(2,end-26+1:end-25+1));
%         VHT_SIG1=fliplr((B(3,end-24+1:end)));
%         VHT_SIG2=fliplr((B(4,end-24+1:end)));
%         VHT_SIGB=fliplr((B(5,end-24+1:end)));
%         C_i=B(6:5+256,17:32);
%         C_r=B(6:5+256,1:16);
%         D_i=bin2dec(C_r);
%         D_r=bin2dec(C_i);
%         D_i(D_i>2^15)=D_i(D_i>2^15)-2^16;
%         D_r(D_r>2^15)=D_r(D_r>2^15)-2^16;
%         CHEST=D_r+1i*D_i;
%
%         L_LENGTH=bin2dec(fliplr(L_SIG(6:17)));
%
%
%         if RX_FORMAT==3
%             SU=bin2dec(VHT_SIG1(5:10))==0|bin2dec(VHT_SIG1(5:10))==63;
%             if SU
%                 MCS=bin2dec(fliplr(VHT_SIG2(5:8)));
%                 LENGTH=bin2dec(VHT_SIGB(1:17));
%             else
%                 MCS=0;
%                 LENGTH=bin2dec(fliplr(VHT_SIGB(1:16)));
%             end
%         elseif RX_FORMAT==1
%             MCS=bin2dec(fliplr(VHT_SIG1(1:7)));
%             LENGTH=bin2dec(fliplr(VHT_SIG1(9:24)));
%
%         else
%             switch L_SIG(1:4)
%                 case '1101'
%                     MCS=0;
%                 case '1111'
%                     MCS=1;
%                 case '0101'
%                     MCS=2;
%                 case '0111'
%                     MCS=3;
%                 case '1001'
%                     MCS=4;
%                 case '1011'
%                     MCS=5;
%                 case '0001'
%                     MCS=6;
%                 case '0011'
%                     MCS=7;
%             end
%             LENGTH=L_LENGTH;
%         end
%
%         if MODE==0
%             C_i=B(6+256:end,17:32);
%             C_r=B(6+256:end,1:16);
%             D_i=bin2dec(C_i);
%             D_r=bin2dec(C_r);
%             D_i(D_i>2^15)=D_i(D_i>2^15)-2^16;
%             D_r(D_r>2^15)=D_r(D_r>2^15)-2^16;
%             CONST=D_r/2^12+1i*D_i/2^12;
%             if MCS==0||RX_FORMAT==0&&MCS==1
%             MSE=num2str(-10*log10(mean(abs(real(CONST)-round(real(CONST))).^2)));
%             elseif MCS<=2
%             MSE=num2str(-10*log10(mean(abs((CONST)-round((CONST*sqrt(2)))/sqrt(2)).^2)));
%             else
%             MSE='UNKNOWN';
%             end
%             plot(real(CONST),imag(CONST),'*');
%
%             axis([-1.5 1.5 -1.5 1.5])
%             text(-1.3,1.4,['FORMAT = ' num2str(RX_FORMAT) ]);
%             text(-1.3,1.2,['TIME STAMP = ' num2str(TIME_STAMP/PHY_SAMP)])
%             text(-1.3,1.0,['MCS = ' num2str(MCS)])
%             text(-1.3,0.8,['LENGTH = ' num2str(LENGTH)])
%              text(-1.3,0.6,['MSE = ' MSE])
%
%             drawnow limitrate
%         else
%             MSDU= B(6+256:end,:);
%             DURATIONID=MSDU(1,1:16);
%             FC=fliplr(MSDU(1,17:32));
%             temp1=dec2hex(bin2dec([MSDU(2,25:32) MSDU(2,17:24) MSDU(2,9:16) MSDU(2,1:8)]));
%             temp2=dec2hex(bin2dec([MSDU(3,25:32) MSDU(3,17:24)]));
%             ADDR1=[temp1 temp2];
%             temp1=dec2hex(bin2dec([MSDU(3,9:16) MSDU(3,1:8)]));
%             temp2=dec2hex(bin2dec([MSDU(4,25:32) MSDU(4,17:24) MSDU(4,9:16) MSDU(4,1:8)]));
%             ADDR2=[temp1 temp2];
%             if strcmp(FC(1:8),'00000001')
%                 FRAME='BEACON';
%                             elseif strcmp(FC(1:4),'0000')
%                 FRAME='MNGT FRAME';
%             elseif strcmp(FC(1:4),'0010')
%                 FRAME='CTRL FRAME';
%             elseif strcmp(FC(1:4),'0001')
%                 FRAME='DATA FRAME';
%             end
%             plot(abs((CHEST)));
%             text(10,3500,['FORMAT = ' num2str(RX_FORMAT) ]);
%             text(10,3300,['TIME STAMP = ' num2str(TIME_STAMP/PHY_SAMP)])
%             text(10,3100,['MCS = ' num2str(MCS)])
%             text(10,2900,['LENGTH = ' num2str(LENGTH)])
%             text(10,2700,['FRAME = ' FRAME])
%             text(10,2500,['ADDR1 (Destination) = ' ADDR1])
%             text(10,2300,['ADDR2 (Source = ' ADDR2])
%
%             axis([0 256 0 4000])
%             drawnow limitrate
%         end
%         %             flushinput(u);
%
%     end
% end



