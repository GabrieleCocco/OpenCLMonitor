using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace OpenCLDotNetMonitor
{
    enum OpCodes
    {
        CREATE,
        OK,
        ENUMERATE_COUNTERS,
        PERF_INIT,
        RELEASE,
        GET_COUNTERS,
        END
    }

    public class MonitorMessage
    {
        /// <summary>
        /// the message type. 
        /// </summary>
        public OpCodes OpCode { get; set; }
        /// <summary>
        /// the message type as row integer value
        /// </summary>
        public int OpCode_Raw { get; set; }
        /// <summary>
        /// 0 if successful, non zero otherwise
        /// </summary>
        public int As { get; set; }
        /// <summary>
        /// error code if As != 0
        /// </summary>
        public int Aps { get; set; }
        /// <summary>
        /// the content of the message
        /// </summary>
        public string[] Body { get; set; }

        private MonitorMessage()
        {
        }
        
        /// <summary>
        /// Parse a MonitorMessage out of a string received on the pipe
        /// </summary>
        /// <param name="rawString">the string received on the pipe</param>
        /// <returns></returns>
        public static MonitorMessage ParseFromString(string rawString)
        {
            MonitorMessage m = new MonitorMessage();
            string[] tags = rawString.Split(new string[] { " " }, StringSplitOptions.RemoveEmptyEntries);
            if (tags.Length < 3)
                throw new ArgumentException("raw string does not contain the minimum number of needed arguments");
            int opcode = -1;
            if (int.TryParse(tags[0], out opcode))
            {
                m.OpCode_Raw = opcode;
                m.OpCode = (OpCodes)opcode;
            }
            else
            {
                throw new ArgumentException("the first argument is not a valid value, not an integer value", "Operation type");
            }
            int err = -1;
            if (int.TryParse(tags[1], out err))
            {
                m.As = err;
            }
            else
            {
                throw new ArgumentException("the second argument is not a valid value, not an integer value", "As");
            }
            int errDesc = 0;
            if (int.TryParse(tags[2], out errDesc))
            {
                m.Aps = errDesc;
            }
            else
            {
                throw new ArgumentException("the third argument is not a valid value, not an integer value", "Aps");
            }
            if (tags.Length > 3)
            {
                m.Body = tags.Skip(3).ToArray();
            }
            else
            {
                m.Body = new string[] { };
            }
            return m;            
        }

        /// <summary>
        /// create an instance of a MonitorMessage with a list of strings as the message body
        /// </summary>
        /// <param name="_op">the operation type</param>
        /// <param name="_as">0 if operation was succesfull, non zero otherwise</param>
        /// <param name="_aps">error code, 0 if not error</param>
        /// <param name="_body">body of the message</param>
        public MonitorMessage(OpCodes _op, int _as, int _aps, string[] _body)
        {
            MonitorMessage m = new MonitorMessage();
            m.Body = _body;
            m.As = _as;
            m.Aps = _aps;
            m.OpCode = _op;
            m.OpCode_Raw = (int) _op;
        }

        /// <summary>
        /// create an instance of a MonitorMessage with a list of floats as the message body
        /// </summary>
        /// <param name="_op">the operation type</param>
        /// <param name="_as">0 if operation was succesfull, non zero otherwise</param>
        /// <param name="_aps">error code, 0 if not error</param>
        /// <param name="_body">body of the message</param>
        public MonitorMessage(OpCodes _op, int _as, int _aps, float[] _body)
        {
            MonitorMessage m = new MonitorMessage();

            m.Body = _body.Select(x => x.ToString()).ToArray() ;
            m.As = _as;
            m.Aps = _aps;
            m.OpCode = _op;
            m.OpCode_Raw = (int)_op;
        }

        /// <summary>
        /// create an instance of a MonitorMessage with a single string as message body
        /// </summary>
        /// <param name="_op">the operation type</param>
        /// <param name="_as">0 if operation was succesfull, non zero otherwise</param>
        /// <param name="_aps">error code, 0 if not error</param>
        /// <param name="_body">body of the message</param>
        public MonitorMessage(OpCodes _op, int _as, int _aps, string _body)
        {
            MonitorMessage m = new MonitorMessage();

            m.Body = new string[] { _body };
            m.As = _as;
            m.Aps = _aps;
            m.OpCode = _op;
            m.OpCode_Raw = (int)_op;
        }

        /// <summary>
        /// get a string usable on the pipe from this MonitorMessage
        /// </summary>
        /// <returns></returns>
        public override string ToString()
        {
            StringBuilder sb = new StringBuilder();
            sb.AppendFormat("{0} {1} {2} ", OpCode_Raw, As, Aps);
            foreach (string b in Body)
            {
                sb.AppendFormat("{0} ", b);
            }
            // remove last space
            if (Body.Length > 0)
                sb.Remove(sb.Length - 1, 1);
            return sb.ToString();
        }

    }
}
