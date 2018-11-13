class NormalizedArgs {
	String fmt;
	Object[] args;

	NormalizedArgs(String fmt, Object[] args) {
		this.fmt = fmt;
		this.args = args;
	}
}

class J2CHelper
{
	final private static char af_none = 'n';
	final private static char af_bool = 'b';
	final private static char af_int  = 'i';
	final private static char af_double  = 'd';
	final private static char af_lstring = 'S';
	final private static char af_buffer  = 'B';
	final private static char af_jarray  = 'a';
	final private static char af_jobject = 'o';

	public static NormalizedArgs normalizeArgs(Object ...args) throws Exception {
		if (args == null) {
			return new NormalizedArgs("", null);
		}

		Object[] res = new Object[args.length];
		StringBuffer fmt = new StringBuffer(args.length);
		for (int i=0; i<args.length; i++) {
			Object obj = args[i];
			if (obj == null) {
				fmt.append(af_none);
				res[i] = null;
				continue;
			}

			if (obj instanceof String) {
				fmt.append(af_lstring);
				res[i] = ((String)obj).getBytes("utf-8");
			} else if (obj instanceof byte[]) {
				fmt.append(af_lstring);
				res[i] = obj;
			} else if (obj instanceof Integer) {
				fmt.append(af_int);
				res[i] = obj;
			} else if (obj instanceof Boolean) {
				fmt.append(af_bool);
				res[i] = obj;
			} else if (obj instanceof Double) {
				fmt.append(af_double);
				res[i] = obj;
			} else if (obj instanceof ObjArg) {
				ObjArg objarg = (ObjArg)obj;
				switch (objarg.type) {
				case ObjArg.af_string:
					fmt.append(af_lstring);
					break;
				case ObjArg.af_buffer:
					fmt.append(af_buffer);
					break;
				case ObjArg.af_jarray:
					fmt.append(af_jarray);
					break;
				case ObjArg.af_jobject:
					fmt.append(af_jobject);
					break;
				default:
					throw new Exception("unknown arg type " + objarg.type);
				}
				res[i] = objarg.arg;
			} else {
				throw new Exception("your object type is not supported");
			}
		}

		return new NormalizedArgs(fmt.toString(), res);
	}
}
