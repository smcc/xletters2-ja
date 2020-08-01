/** Analyze Japanese words and establish a correspondence between the
 * Kanji writing and the Kana reading, based on the readings of the
 * Kanji making up the word. To run this on your own Japanese-XX
 * dictionary, you will need Jim Breen's Kanji Dictionary.
 *
 * This is free software published under the terms of the GNU public
 * license. copyright Adrian Daerr, Paris 2004
 */

import java.io.*;
import java.util.*;

public class CompJapanEngDict {
    Hashtable kdic;
    Vector nkanav = new Vector();
    final Character.UnicodeBlock HIRAGANA = Character.UnicodeBlock.HIRAGANA;
    final Character.UnicodeBlock KATAKANA = Character.UnicodeBlock.KATAKANA;
    BufferedWriter out;
    {
	System.err.println("Force UTF-8 encoding on stdout...");
	try {// bugware-alarm! The default encoding should be used
	    // instead of forcing utf-8; but how can the user change
	    // the default stdout encoding when launching the JavaVM ?
	    // LC_CTYPE appears to be ignored on MacOS 10.3.+Java 1.4.1
	    out = new BufferedWriter(new OutputStreamWriter(System.out,"UTF-8"));
	} catch (UnsupportedEncodingException e) {
	    System.err.println("Can't use explicit UTF-8 output, using stdout in default encoding");
	    out = new BufferedWriter(new OutputStreamWriter(System.out));
	}
    }

    public static void main(String args[])
        throws FileNotFoundException, IOException {
        if (args.length==0) {
            System.err.println("usage: CompJapanEngDict dict-file");
        } else {
	    new CompJapanEngDict().run(args);
        }
    }

    public void run(String args[])
        throws FileNotFoundException, IOException {
	int i;//the usual insignificant variable

        // open kanji dictionary
        readKanjiDict();

        // open japanese-english dictionary
        LineNumberReader jpendictf = new LineNumberReader(new InputStreamReader(new FileInputStream(args[0]),"UTF-8"));
        BufferedWriter outdictf = new BufferedWriter(new OutputStreamWriter(new FileOutputStream("out.txt"),"UTF-8"));

        // process line by line
	String ligne;
        while((ligne = jpendictf.readLine()) != null) {
            int posBeg,posEnd;
            // skip lines marked as comments
            if (ligne.startsWith("#") || ligne.length()==0) continue;
            posBeg = ligne.indexOf(" [");// look for KANA part
            if (posBeg < 0) {// conclude that KANJI is already pure KANA
                outdictf.write(ligne+"\n");
            } else {// calculate kanji ending markers
		// isolate and output kanji
                String kanji = ligne.substring(0,posBeg);
                outdictf.write(kanji);
                // substitute kanji repetition mark
		while ((i=kanji.indexOf("\u3005"))>0) {
		    kanji = kanji.substring(0,i)+kanji.charAt(i-1)+kanji.substring(i+1);
		}
		// count kanji
		int nKanji=0;
		for (i=0; i<kanji.length(); i++) {
		    if (! isKana(kanji.charAt(i))) nKanji++;
		}
                do {
                    posEnd = ligne.indexOf("]",posBeg);
                    //System.out.println("pos:"+posBeg+":"+posEnd);
                    String kana = ligne.substring(posBeg+2,posEnd);
		    if ((kana.indexOf(".")<0)&&(nKanji>1)) {
			nkanav.clear();//no separation marks yet
			findKanaSep(kanji,kana,"");
			if (nkanav.size()==0) {
			    // no decomposition found: be verbose...
			    println("line #"+(jpendictf.getLineNumber()-1)+
				    ": don't know how to decompose "+
				    kanji+" = "+kana);
			    for (i=0; i<kanji.length(); i++) {
				Object nK = (Object)new Character(kanji.charAt(i));
				if (kdic.containsKey(nK)) {
				    HashSet readings = (HashSet)((Vector)kdic.get(nK)).elementAt(1);
				    println("  with "+nK+" = "+readings.toString());
				}

			    }
			    // ... and write kana unchanged
			    outdictf.write(" ["+kana+"]");
			} else {
			    if (nkanav.size()>1) {
				// too many decompositions found
				println("line #"+(jpendictf.getLineNumber()-1)+
					": decomposition of "+kanji+
					" ambiguous (leaving undecided):");
				for (i=0; i<nkanav.size(); i++)
				    println("  (#"+i+") "+(String)nkanav.elementAt(i)+" ?");
				// write kana unchanged
				outdictf.write(" ["+kana+"]");
			    } else {
				// everything went smoothly
				outdictf.write(" ["+(String)nkanav.firstElement()+"]");
			    }
			}
		    } else {
			// nothing to be done
			if (nKanji>1) {
			    println("line #"+(jpendictf.getLineNumber()-1)+
				    ": already separated, copying unchecked: "+
				    kanji+" = "+kana);
			} else {
			    println("line #"+(jpendictf.getLineNumber()-1)+
				    ": trivial to decompose, copying: "+
				    kanji+" = "+kana);
			}
			outdictf.write(" ["+kana+"]");
		    }
                } while((posBeg = ligne.indexOf(" [",posBeg+1))>0);
                outdictf.write(ligne.substring(posEnd+1));
		// now calculate META-information
		int f=0, g=0;
		for (i=0; i<kanji.length(); i++) {
		    Object nK = (Object)new Character(kanji.charAt(i));
		    if (kdic.containsKey(nK)) {
			StringTokenizer meta = new StringTokenizer((String)((Vector)kdic.get(nK)).elementAt(3));
			while (meta.hasMoreTokens()) {
			    String st = meta.nextToken();
			    if (st.charAt(0)=='F') {//frequency-of-use ranking
				try {
				    f=Math.max(f,Integer.parseInt(st.substring(1),10));
				} catch (NumberFormatException e) {
				    System.err.println("line #"+(jpendictf.getLineNumber()-1)+
						       " NumberFormatException for F-field");
				    f=0;
				}
			    } else if (st.charAt(0)=='G') {//Jouyou-grade level
				try {
				    g=Math.max(g,Integer.parseInt(st.substring(1),10));
				} catch (NumberFormatException e) {
				    System.err.println("line #"+(jpendictf.getLineNumber()-1)+
						       " NumberFormatException for G-field");
				    g=0;
				}
			    }
			}
		    }

		}
		if (f>0) { outdictf.write(" <F"+f+">"); }
		if (g>0) { outdictf.write(" <G"+g+">"); }
                outdictf.write("\n");
            }

        }
        jpendictf.close();
        outdictf.close();

        //Enumeration e = pointsGPS.elements();
        //while (e.hasMoreElements())
        //    System.out.println(e.nextElement());
	out.close();
    }

    void readKanjiDict() 
        throws FileNotFoundException, IOException {
        kdic = new Hashtable(10000);
        // open kanji dictionary
        BufferedReader kdicf = new BufferedReader(new InputStreamReader(new FileInputStream("kanjidic.txt"),"UTF-8"));
        String ligne;
	Vector entry;// contains verbatim line and list of readings for kanji
        HashSet readings;// list of readings for given kanji
	StringBuffer meta;// information about kanji
	StringBuffer english;// english meanings

        // read kanji and definition
        while((ligne = kdicf.readLine()) != null) {
            // skip lines marked as comments
            if (ligne.startsWith("#") || ligne.length()<3) continue;
	    // find readings
	    readings = new HashSet();
	    meta = new StringBuffer();
	    english = new StringBuffer();
	    StringTokenizer st = new StringTokenizer(ligne);
	    st.nextToken();//skip kanji
	    while (st.hasMoreTokens()) {
		String token = st.nextToken();
		if (token.charAt(0)=='{') {// english meaning ?
		    english.append(" "+token);
		    while ((token.indexOf("}")<0) && (st.hasMoreTokens())) {
			token = st.nextToken();
			english.append(" "+token);
		    }
		    continue;
		}
		if (isKana(token.charAt(0))) {// yet another reading?
		    // convert to hiragana and add
		    StringBuffer hiraganaToken = new StringBuffer();
		    for (int i=0; i<token.length(); i++) {
			char c=token.charAt(i);
			if (isHiragana(c)) {
			    hiraganaToken.append(c);
			} else if (isKatakana(c)) {//convert to hiragana
			    hiraganaToken.append((char)(c-0x60));

			} else break;
		    }
		    token = hiraganaToken.toString();
		    readings.add(token);
		    // add possible modifications of readings
		    char c=token.charAt(0);
		    // tell if kana may soften (vocalize) in compounds
		    if (c=='\u304b' || //ka
			c=='\u304d' || //ki
			c=='\u304f' || //ku
			c=='\u3051' || //ke
			c=='\u3053' || //ko
			c=='\u3055' || //sa
			c=='\u3057' || //shi
			c=='\u3059' || //su
			c=='\u305b' || //se
			c=='\u305d' || //so
			c=='\u305f' || //ta
			c=='\u3061' || //chi
			c=='\u3064' || //tsu
			c=='\u3066' || //te
			c=='\u3068' || //to
			c=='\u306f' || //ha
			c=='\u3072' || //hi
			c=='\u3075' || //fu
			c=='\u3078' || //he
			c=='\u307b') {; //ho
			readings.add(""+((char)(token.charAt(0)+1))+
				     token.substring(1));
		    }
		    // h-series kana may harden in compounds (1)
		    if (c=='\u306f' || //ha
			c=='\u3072' || //hi
			c=='\u3075' || //fu
			c=='\u3078' || //he
			c=='\u307b') {; //ho
			readings.add(""+((char)(token.charAt(0)+2))+
				     token.substring(1));
		    }
		    // h-series kana may harden in compounds (2)
		    if (c=='\u3070' || //ba
			c=='\u3073' || //bi
			c=='\u3076' || //bu
			c=='\u3079' || //be
			c=='\u307c') {; //bo
			readings.add(""+((char)(token.charAt(0)+1))+
				     token.substring(1));
		    }
		    // trailing tsu tends to "become small"
		    if (token.charAt(token.length()-1)=='\u3064') {//tsu
			readings.add(token.substring(0,token.length()-1)+
				     '\u3063');
		    }
		} else {// meta-info
		    if (meta.length()>0) { meta.append(" "); }
		    meta.append(token);
		}
	    }
	    entry = new Vector(4);
	    entry.add((Object)ligne);
	    entry.add((Object)readings);
	    entry.add((Object)english.toString());
	    entry.add((Object)meta.toString());
	    //out.write(entry.toString()+"\n");
            kdic.put((Object)new Character(ligne.charAt(0)),(Object)entry);
        }
        kdicf.close();
        System.err.println("Read "+kdic.size()+" kanji entries");
	return;
    }

    void findKanaSep(String kanji, String kana, String sepkana)
	throws IOException {
	//println("kanji="+kanji+", kana="+kana+", sep="+sepkana);
	if ((kanji.length() == 0) || (kana.length() == 0)) {
	    if ((kanji.length() == 0) && (kana.length() == 0)) {
		nkanav.add(sepkana.endsWith(".")?
			   sepkana.substring(0,sepkana.length()-1):
			   sepkana);
	    }
	    return;
	}
	char nextKanjiChar = kanji.charAt(0);
	Character nextKanjiC = new Character(nextKanjiChar);
	String nextKanjiS = nextKanjiC.toString();
	if (isKana(nextKanjiChar)) {
	    if (kanji.charAt(0) == kana.charAt(0)) {
		findKanaSep(kanji.substring(1), kana.substring(1),
			    sepkana+nextKanjiS);
	    }
	} else if (kdic.containsKey((Object)nextKanjiC)) {
	    HashSet readings = (HashSet)((Vector)kdic.get((Object)nextKanjiC)).elementAt(1);
	    Iterator rIter = readings.iterator();
	    //println(readings.toString());
	    while (rIter.hasNext()) {
		String r = (String)rIter.next();
		if (kana.startsWith(r)) {
		    findKanaSep(kanji.substring(1), kana.substring(r.length()),
				sepkana+r+".");
		}
	    }
	} else {
	    println("character <"+nextKanjiS+"> neither a Kana nor in Kanji-Dict");
	    return;
	}
    }

    /** find out if character is HIRAGANA or KATAKANA */
    boolean isKana(char c) {
	final Character.UnicodeBlock type = Character.UnicodeBlock.of(c);
	return (type.equals(HIRAGANA) || type.equals(KATAKANA));
    }

    /** find out if character is HIRAGANA */
    boolean isHiragana(char c) {
	return Character.UnicodeBlock.of(c).equals(HIRAGANA);
    }

    /** find out if character is KATAKANA */
    boolean isKatakana(char c) {
	return Character.UnicodeBlock.of(c).equals(KATAKANA);
    }

    void println(String s) {
	try {
	    out.write(s);
	    out.newLine();
	} catch (IOException e) {
	    System.err.println("caught IOException while trying to print:");
	    System.err.println(s);
	    e.printStackTrace();
	}
    }
}
