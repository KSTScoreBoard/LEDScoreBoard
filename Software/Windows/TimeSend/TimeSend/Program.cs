using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Timers;
using System.IO.Ports;

namespace TimeSend
{
    class Program
    {
        static void Main(string[] args)
        {
            Timer timer = new Timer(500);
            timer.Elapsed += Timer_Elapsed;
            timer.Start();
            SerialPort.Open();
            Console.ReadLine();
        }
        static SerialPort SerialPort = new SerialPort("COM10", 115200, Parity.None, 8, StopBits.One);

        private static void Timer_Elapsed(object sender, ElapsedEventArgs e)
        {
            
            Send();
        }

        public static void Send()
        {
            DateTime time = DateTime.Now;
            Console.WriteLine(time);
            byte[] header = new byte[] { 0xA5, 0x5A, 0x80, 0x0D };
            byte[] data = new byte[] {0x01,(byte)(time.Second),(byte)(time.Second >> 8),0
                                          ,(byte)(time.Minute),(byte)(time.Minute >> 8),0
                                          ,(byte)(0),(byte)(0 >> 8),0
                                          ,(byte)(0),(byte)(0 >> 8),0};
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
}
