public class JSTest {
	public static void main(String args[]) throws Exception {
		if (args.length < 2) {
			System.err.println("Usage: java JSTest <js_file> <func>");
			return;
		}

		DukBridge js = new DukBridge(".", null);
		if (!js.registerFileFunc(args[0], args[1])) {
			System.err.println("failed to register");
			return;
		}

		Object b;
		if (args.length == 2) {
			b = js.callFunc(args[1]);
		} else {
			Object[] argv = new Object[args.length-2];
			for (int i=0, j=2; j<args.length; i++, j++) {
				argv[i] = args[j];
			}
			b = js.callFunc(args[1], argv); 
		}

		if (b == null) {
			System.out.println("no result");
		} else if (b instanceof byte[]) {
			System.out.println(new String((byte[])b, "utf-8"));
		} else {
			System.out.println(b);
		}
	}
}
