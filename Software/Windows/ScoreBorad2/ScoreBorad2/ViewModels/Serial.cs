using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO.Ports;

namespace ScoreBorad2.ViewModels
{
    class Serial
    {
        private int Score1;
        private int Score2;
        private int Score3;
        private int Score4;

        private Mode Mode1;
        private Mode Mode2;
        private Mode Mode3;
        private Mode Mode4;

        private SerialPort SerialPort;
        public Serial(SerialPort serialPort)
        {
            this.SerialPort = serialPort;
        }

        public void SetScore(int _score1,int _score2,int _score3,int _score4)
        {
            this.Score1 = _score1;
            this.Score2 = _score2;
            this.Score3 = _score3;
            this.Score4 = _score4;
        }

        public void SetMode(Mode _mode1,Mode _mode2,Mode _mode3,Mode _mode4)
        {
            this.Mode1 = _mode1;
            this.Mode2 = _mode2;
            this.Mode3 = _mode3;
            this.Mode4 = _mode4;
        }

        public void Send()
        {
            byte[] header = new byte[] { 0xA5, 0x5A, 0x80, 0x0D };
            byte[] data = new byte[] {0x01,(byte)(Score1),(byte)(Score1 >> 8),Mode1.ModeBit()
                                          ,(byte)(Score2),(byte)(Score2 >> 8),Mode2.ModeBit()
                                          ,(byte)(Score3),(byte)(Score3 >> 8),Mode3.ModeBit()
                                          ,(byte)(Score4),(byte)(Score4 >> 8),Mode4.ModeBit()};
            byte xor = data[0];
            for (int i = 1; i < data.Length; i++)
            {
                xor = (byte)(xor ^ data[i]);
            }
            byte[] checksum = new byte[] { xor };

            byte[] vs = new byte[header.Length + data.Length + checksum.Length];
            header.CopyTo(vs, 0);
            data.CopyTo(vs, header.Length);
            checksum.CopyTo(vs, header.Length + data.Length);
            try
            {
                SerialPort.Write(vs, 0, vs.Length);
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
        }
        
    }

    public class Mode
    {
        public bool scroll1;
        public bool scroll10;
        public bool scroll100;
        public bool disable;

        public Mode(bool scroll1,bool scroll10,bool scroll100,bool disable)
        {
            this.scroll1 = scroll1;
            this.scroll10 = scroll10;
            this.scroll100 = scroll100;
            this.disable = disable;
        }

        public byte ModeBit()
        {
            return (byte)((Convert.ToUInt16(scroll1) << 1) + (Convert.ToUInt16(scroll10) << 2) + (Convert.ToUInt16(scroll100) << 3) + (Convert.ToUInt16(disable)));
        }
    }
}
