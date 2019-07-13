----------------------------------------------------------------------------------
--
-- C64 - Pi Interface CPLD Firmware version 1.0, July 2019 are
-- Copyright (c) 2019 Kim Jorgensen, are derived from EasyFlash 3 CPLD Firmware 1.1.1,
-- and are distributed according to the same disclaimer and license as
-- EasyFlash 3 CPLD Firmware 1.1.1
-- 
-- EasyFlash 3 CPLD Firmware versions 0.9.0, December 2011, through 1.1.1, August 2012, are
-- Copyright (c) 2011-2012 Thomas 'skoe' Giesel
--
-- This software is provided 'as-is', without any express or implied
-- warranty.  In no event will the authors be held liable for any damages
-- arising from the use of this software.
--
-- Permission is granted to anyone to use this software for any purpose,
-- including commercial applications, and to alter it and redistribute it
-- freely, subject to the following restrictions:
--
-- 1. The origin of this software must not be misrepresented; you must not
--    claim that you wrote the original software. If you use this software
--    in a product, an acknowledgment in the product documentation would be
--    appreciated but is not required.
-- 2. Altered source versions must be plainly marked as such, and must not be
--    misrepresented as being the original software.
-- 3. This notice may not be removed or altered from any source distribution.
--
----------------------------------------------------------------------------------


library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity cpi is
    port (
           addr:                in std_logic_vector (15 downto 0);
           data:                inout std_logic_vector (7 downto 0);
           n_dma:               out std_logic;
           ba:                  in std_logic;
           n_roml:              in std_logic;
           n_romh:              in std_logic;
           n_io1:               in std_logic;
           n_io2:               in std_logic;
           n_wr:                in std_logic;
           n_irq:               out std_logic;
           n_nmi:               out std_logic;
           n_reset:             in std_logic;
           d_clk:               in std_logic;
           clk:                 in std_logic;
           phi2:                in std_logic;
           n_exrom:             out std_logic;
           n_game:              out std_logic;
           n_sw2:               in  std_logic;
           n_led:               out std_logic;
           sa:                  in std_logic_vector (5 downto 0);
           sd:                  out std_logic_vector (15 downto 0);
           sdrr:                out std_logic;  -- SD16
           sdwr:                out std_logic;  -- SD17
           n_soe:               in std_logic;
           n_swe:               in std_logic;
           gpio:                in std_logic_vector (27 downto 26);
           n_flash_cs:          out std_logic;
           n_ram_cs:            out std_logic;
           sck:                 out std_logic;
           miso:                out std_logic;
           n_wp:                out std_logic;
           mosi:                out std_logic;
           n_hold:              out std_logic;           
           tp1:                 out std_logic;
           tp2:                 out std_logic;
           tp3:                 out std_logic;
           tp4:                 out std_logic;
           tp5:                 out std_logic
         );
end cpi;

architecture cpi_arc of cpi is
    signal rd:                  std_logic;
    signal wr:                  std_logic;
    signal rp:                  std_logic;
    signal wp:                  std_logic;
    signal wp_end:              std_logic;
    signal cycle_time:          std_logic_vector(10 downto 0);
    signal cycle_start:         std_logic;

    signal data_out:            std_logic_vector(7 downto 0);
    signal data_out_valid:      std_logic;

    signal n_exrom_out:         std_logic;
    signal n_game_out:          std_logic;

    component exp_bus_ctrl is
        port (
            clk:                in  std_logic;
            phi2:               in  std_logic;
            n_wr:               in  std_logic;
            rd:                 out std_logic;
            wr:                 out std_logic;
            rp:                 out std_logic;
            wp:                 out std_logic;
            wp_end:             out std_logic;
            cycle_time:         out std_logic_vector(10 downto 0);
            cycle_start:        out std_logic
        );
    end component;

    signal clk_div2:            std_logic;

    signal addr_i:              std_logic_vector (15 downto 0);
    signal data_i:              std_logic_vector (7 downto 0);
    signal n_wr_i:              std_logic;
    signal ba_i:                std_logic;
    
    signal addr_read:           std_logic;
    signal data_read:           std_logic;

--    signal test_register:       std_logic_vector(7 downto 0);

begin
    ---------------------------------------------------------------------------
    -- Components
    ---------------------------------------------------------------------------
    u_exp_bus_ctrl: exp_bus_ctrl port map
    (
        clk                     => clk,
        phi2                    => phi2,
        n_wr                    => n_wr,
        rd                      => rd,
        wr                      => wr,
        rp                      => rp,
        wp                      => wp,
        wp_end                  => wp_end,
        cycle_time              => cycle_time,
        cycle_start             => cycle_start
    );

    ---------------------------------------------------------------------------
    -- divider by two - TODO: Move to new component
    ---------------------------------------------------------------------------

    p_clk_div2: process(clk)
    begin
      if rising_edge(clk) then
        if n_reset = '0' then
          clk_div2 <= '0';
        else
          clk_div2 <= not clk_div2;
        end if;
      end if;
    end process p_clk_div2;

    -- SPI Flash & RAM - TODO: Move to new component
    sck         <= clk_div2;
    n_ram_cs    <= '1';
    n_flash_cs  <= '1';
    n_wp        <= '1';
    n_hold      <= '1';
    mosi        <= '1';
    miso        <= 'Z';

    -- unused signals and defaults
    n_exrom <= 'Z';
    n_game  <= 'Z';
    n_dma   <= 'Z';
    n_nmi   <= 'Z';
    n_irq   <= 'Z';

    tp1     <= 'Z';
    tp2     <= 'Z';
    tp3     <= 'Z';
    tp4     <= 'Z';
    tp5     <= 'Z';

    sdwr    <= 'Z';

    ---------------------------------------------------------------------------
    -- LED / button test
    ---------------------------------------------------------------------------
--    led_test: process(clk, n_reset, n_sw2)
--    begin
--        if n_reset = '0' then
--            n_led <= '1';
--        elsif rising_edge(clk) then
--            if n_sw2 = '0' then
--                n_led <= '0';
--            elsif n_io1 = '0' and wp = '1' then
--                n_led <= not data(0);            
--            end if;
--        end if;
--    end process;

    ---------------------------------------------------------------------------
    -- Write to test register 1 at $de00-$deff
    ---------------------------------------------------------------------------
--    w_test_reg1: process(clk, n_reset, n_io1, wp)
--    begin
--        if n_reset = '0' then
--            test_register <= x"00";
--        elsif rising_edge(clk) then
--            if n_io1 = '0' and wp = '1' then
--                test_register <= data; 
--            end if;
--        end if;
--    end process;

    ---------------------------------------------------------------------------
    -- Read from test register 1 + 2 at $df00-$dfff
    ---------------------------------------------------------------------------
--    r_test_reg: process(n_io1, n_io2, rd)
--    begin
--        data_out_valid <= '0';
--        if n_io1 = '0' and rd = '1' then
--            data_out <= test_register;
--            data_out_valid <= not cycle_start;
--        elsif n_io2 = '0' and rd = '1' then
--            data_out <= addr(7 downto 0);
--            data_out_valid <= not cycle_start;
--        end if;
--    end process;

    ---------------------------------------------------------------------------
    -- Capture C64 bus in register
    ---------------------------------------------------------------------------
    smi_capture_bus: process(n_reset, phi2)
    begin
        if n_reset = '0' then
            addr_i      <= (others => '0');
            data_i      <= (others => '0');
            n_wr_i      <= '0';
            ba_i        <= '0';
        elsif falling_edge(phi2) then
            addr_i      <= addr;        -- TODO: Add overflow flag?
            data_i      <= data;
            n_wr_i      <= n_wr;
            ba_i        <= ba;
        end if;
    end process;

    ---------------------------------------------------------------------------
    -- Set external DMA read request - TODO: Make 'Z' when not active?
    ---------------------------------------------------------------------------  
    smi_data_ready: process(n_reset, clk, phi2, data_read, addr_read)
    begin
        if n_reset = '0' then
            sdrr <= '0';
        elsif data_read = '1' then
            sdrr <= '0';
        elsif rising_edge(clk) then
            if phi2 = '0' and (addr_read = '0' or data_read = '0') then
                sdrr <= '1';
            elsif addr_read = '1' and data_read = '0' then
                sdrr <= '1';
            else
                sdrr <= '0';
            end if;
        end if;
    end process;

    ---------------------------------------------------------------------------
    -- Read C64 bus from register
    --------------------------------------------------------------------------- 
    smi_oe: process(n_reset, n_soe, phi2, cycle_time, addr_read)
    begin
        if n_reset = '0' then
            addr_read <= '1';
            data_read <= '1';
        elsif phi2 = '1' and cycle_time(9) = '1' then
            addr_read <= '0';
            data_read <= '0';        
        elsif falling_edge(n_soe) then
            if addr_read = '0' then         -- TODO: Add underflow flag?
                addr_read <= '1';
            else
                data_read <= '1';
            end if;
        end if;
    end process;

    smi_read: process(n_soe, data_read)
    begin
        if n_soe = '0' and data_read = '0' then
            sd <= addr_i;
        elsif n_soe = '0' and data_read = '1' then
            sd <= "000000" & not ba_i & not n_wr_i & data_i;
        else
            sd <= (others => 'Z');
        end if;
    end process;

    n_led <= sa(0);

    ---------------------------------------------------------------------------
    -- Combinatorially decide:
    -- - If we put the memory bus onto the C64 data bus
    -- - If we put data out onto the C64 data bus
    -- - If we put the C64 data bus onto the memory bus
    --
    -- The C64 data bus is only driven if it is a read access with any
    -- of the four Expansion Port control lines asserted.
    --
    -- The memory bus is always driven by the CPLD when no memory chip has
    -- OE active.
    --
    -- We need a special case with phi2 = '0' for C128 which doesn't set R/W
    -- correctly for Phi1 cycles.
    --
    ---------------------------------------------------------------------------
    data_out_enable: process(n_io1, n_io2, n_roml, n_romh, phi2, n_wr,
                             data_out, data_out_valid, data)
    begin
        data <= (others => 'Z');
        if (n_io1 and n_io2 and n_roml and n_romh) = '0' and
           ((n_wr = '1' and phi2 = '1') or phi2 = '0') then
            if data_out_valid = '1' then
                data <= data_out;
            end if;
        end if;
    end process data_out_enable;

end cpi_arc;
