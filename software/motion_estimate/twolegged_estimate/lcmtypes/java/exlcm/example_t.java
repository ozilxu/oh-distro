/* LCM type definition class file
 * This file was automatically generated by lcm-gen
 * DO NOT MODIFY BY HAND!!!!
 */

package exlcm;
 
import java.io.*;
import java.util.*;
import lcm.lcm.*;
 
public final class example_t implements lcm.lcm.LCMEncodable
{
    public long timestamp;
    public double position[];
    public double orientation[];
    public int num_ranges;
    public short ranges[];
 
    public example_t()
    {
        position = new double[3];
        orientation = new double[4];
    }
 
    public static final long LCM_FINGERPRINT;
    public static final long LCM_FINGERPRINT_BASE = 0x9eb95d2c2f66b618L;
 
    static {
        LCM_FINGERPRINT = _hashRecursive(new ArrayList<Class<?>>());
    }
 
    public static long _hashRecursive(ArrayList<Class<?>> classes)
    {
        if (classes.contains(exlcm.example_t.class))
            return 0L;
 
        classes.add(exlcm.example_t.class);
        long hash = LCM_FINGERPRINT_BASE
            ;
        classes.remove(classes.size() - 1);
        return (hash<<1) + ((hash>>63)&1);
    }
 
    public void encode(DataOutput outs) throws IOException
    {
        outs.writeLong(LCM_FINGERPRINT);
        _encodeRecursive(outs);
    }
 
    public void _encodeRecursive(DataOutput outs) throws IOException
    {
        outs.writeLong(this.timestamp); 
 
        for (int a = 0; a < 3; a++) {
            outs.writeDouble(this.position[a]); 
        }
 
        for (int a = 0; a < 4; a++) {
            outs.writeDouble(this.orientation[a]); 
        }
 
        outs.writeInt(this.num_ranges); 
 
        for (int a = 0; a < this.num_ranges; a++) {
            outs.writeShort(this.ranges[a]); 
        }
 
    }
 
    public example_t(byte[] data) throws IOException
    {
        this(new LCMDataInputStream(data));
    }
 
    public example_t(DataInput ins) throws IOException
    {
        if (ins.readLong() != LCM_FINGERPRINT)
            throw new IOException("LCM Decode error: bad fingerprint");
 
        _decodeRecursive(ins);
    }
 
    public static exlcm.example_t _decodeRecursiveFactory(DataInput ins) throws IOException
    {
        exlcm.example_t o = new exlcm.example_t();
        o._decodeRecursive(ins);
        return o;
    }
 
    public void _decodeRecursive(DataInput ins) throws IOException
    {
        this.timestamp = ins.readLong();
 
        this.position = new double[(int) 3];
        for (int a = 0; a < 3; a++) {
            this.position[a] = ins.readDouble();
        }
 
        this.orientation = new double[(int) 4];
        for (int a = 0; a < 4; a++) {
            this.orientation[a] = ins.readDouble();
        }
 
        this.num_ranges = ins.readInt();
 
        this.ranges = new short[(int) num_ranges];
        for (int a = 0; a < this.num_ranges; a++) {
            this.ranges[a] = ins.readShort();
        }
 
    }
 
    public exlcm.example_t copy()
    {
        exlcm.example_t outobj = new exlcm.example_t();
        outobj.timestamp = this.timestamp;
 
        outobj.position = new double[(int) 3];
        System.arraycopy(this.position, 0, outobj.position, 0, 3); 
        outobj.orientation = new double[(int) 4];
        System.arraycopy(this.orientation, 0, outobj.orientation, 0, 4); 
        outobj.num_ranges = this.num_ranges;
 
        outobj.ranges = new short[(int) num_ranges];
        if (this.num_ranges > 0)
            System.arraycopy(this.ranges, 0, outobj.ranges, 0, this.num_ranges); 
        return outobj;
    }
 
}

