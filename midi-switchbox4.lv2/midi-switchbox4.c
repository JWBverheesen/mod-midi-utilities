/* MIDI switchbox3. Recieves one MIDI signal and channels it through one of its three outputs*/

#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/util.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    PORT_CONTROL_TARGET = 0,
    PORT_ATOM_IN,
    PORT_ATOM_OUT1,
    PORT_ATOM_OUT2,
    PORT_ATOM_OUT3
} PortEnum;

typedef struct {
    // keep track of active notes
    bool active_notes[16*127];

    // previous output
    float was_outgoing;

    // URIDs
    LV2_URID urid_midiEvent;

    // control ports
    const float* port_target;

    // atom ports
    const LV2_Atom_Sequence* port_events_in;
    LV2_Atom_Sequence* port_events_out1;
    LV2_Atom_Sequence* port_events_out2;
    LV2_Atom_Sequence* port_events_out3;
} Data;

// Struct for a 3 byte MIDI event
typedef struct {
    LV2_Atom_Event event;
    uint8_t        msg[3];
} LV2_Atom_MIDI;

static LV2_Handle instantiate(const LV2_Descriptor*     descriptor,
                              double                    rate,
                              const char*               path,
                              const LV2_Feature* const* features)
{
    Data* self = (Data*)calloc(1, sizeof(Data));

    // Get host features
    const LV2_URID_Map* map = NULL;

    for (int i = 0; features[i]; ++i) {
        if (!strcmp(features[i]->URI, LV2_URID__map)) {
            map = (const LV2_URID_Map*)features[i]->data;
            break;
        }
    }
    if (!map) {
        free(self);
        return NULL;
    }

    // Map URIs
    self->urid_midiEvent = map->map(map->handle, LV2_MIDI__MidiEvent);

    return self;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data)
{
    Data* self = (Data*)instance;

    switch (port)
    {
    case PORT_CONTROL_TARGET:
            self->port_target = (const float*)data;
            break;
    case PORT_ATOM_IN:
            self->port_events_in = (const LV2_Atom_Sequence*)data;
            break;
    case PORT_ATOM_OUT1:
            self->port_events_out1 = (LV2_Atom_Sequence*)data;
            break;
    case PORT_ATOM_OUT2:
            self->port_events_out2 = (LV2_Atom_Sequence*)data;
            break;
    case PORT_ATOM_OUT3:
            self->port_events_out3 = (LV2_Atom_Sequence*)data;
            break;
    }
}

static void activate(LV2_Handle instance)
{
    Data* self = (Data*)instance;

    memset(self->active_notes, 0, sizeof(bool)*16*127);
}

static void run(LV2_Handle instance, uint32_t sample_count)
{
    Data* self = (Data*)instance;

    const float target_outgoing = (*self->port_target);

    // Get the capacity
    const uint32_t out_capacity_1 = self->port_events_out1->atom.size;
    const uint32_t out_capacity_2 = self->port_events_out2->atom.size;
    const uint32_t out_capacity_3 = self->port_events_out3->atom.size;
    

    // Write an empty Sequence header to the outputs
    lv2_atom_sequence_clear(self->port_events_out1);
    lv2_atom_sequence_clear(self->port_events_out2);
    lv2_atom_sequence_clear(self->port_events_out3);

    // LV2 is so nice...
    self->port_events_out1->atom.type = self->port_events_in->atom.type;
    self->port_events_out2->atom.type = self->port_events_in->atom.type;
    self->port_events_out3->atom.type = self->port_events_in->atom.type;

    // Send note-offs if target port changed
    if (self->was_outgoing != target_outgoing)
    {
        LV2_Atom_MIDI msg;
        memset(&msg, 0, sizeof(LV2_Atom_MIDI));

        msg.event.body.size = 3;
        msg.event.body.type = self->urid_midiEvent;

        for (int i=16; --i >= 0;)
        {
            for (int j=127; --j >=0;)
            {
                if (self->active_notes[i*127+j])
                {
                    self->active_notes[i*127+j] = false;

                    msg.msg[0] = LV2_MIDI_MSG_NOTE_OFF + i;
                    msg.msg[1] = j;
                        lv2_atom_sequence_append_event(self->port_events_out1,
                                                       out_capacity_1,
                                                       (LV2_Atom_Event*)&msg);
                        lv2_atom_sequence_append_event(self->port_events_out2,
                                                       out_capacity_2,
                                                       (LV2_Atom_Event*)&msg);
                        lv2_atom_sequence_append_event(self->port_events_out3,
                                                       out_capacity_3,
                                                       (LV2_Atom_Event*)&msg);
                    if (self->was_outgoing == 0)
                    {
                        lv2_atom_sequence_append_event(self->port_events_out2,
                                                       out_capacity_2,
                                                       (LV2_Atom_Event*)&msg);
                        lv2_atom_sequence_append_event(self->port_events_out3,
                                                       out_capacity_3,
                                                       (LV2_Atom_Event*)&msg);
                    }
                    if (self->was_outgoing == 1)
                    {
                        lv2_atom_sequence_append_event(self->port_events_out1,
                                                       out_capacity_1,
                                                       (LV2_Atom_Event*)&msg);
                        lv2_atom_sequence_append_event(self->port_events_out3,
                                                       out_capacity_3,
                                                       (LV2_Atom_Event*)&msg);
                    }
                    if (self->was_outgoing == 2)
                    {
                        lv2_atom_sequence_append_event(self->port_events_out1,
                                                       out_capacity_1,
                                                       (LV2_Atom_Event*)&msg);
                        lv2_atom_sequence_append_event(self->port_events_out2,
                                                       out_capacity_2,
                                                       (LV2_Atom_Event*)&msg);
                    }
                }
            }
        }

        self->was_outgoing = target_outgoing;
    }

    // Read incoming events
    LV2_ATOM_SEQUENCE_FOREACH(self->port_events_in, ev)
    {
        if (ev->body.type == self->urid_midiEvent)
        {
            const uint8_t* const msg = (const uint8_t*)(ev + 1);

            const uint8_t channel = msg[0] & 0x0F;
            const uint8_t status  = msg[0] & 0xF0;

            switch (status)
            {
            case LV2_MIDI_MSG_NOTE_ON:
                self->active_notes[channel*127+msg[1]] = true;
                break;
            case LV2_MIDI_MSG_NOTE_OFF:
                self->active_notes[channel*127+msg[1]] = false;
                break;
            default:
                break;
            }
        }

        if (target_outgoing == 0)
        {
            lv2_atom_sequence_append_event(self->port_events_out1,
                                           out_capacity_1,
                                           ev);
        }
        if (target_outgoing == 1)
        {
            lv2_atom_sequence_append_event(self->port_events_out2,
                                           out_capacity_2,
                                           ev);
        }
        if (target_outgoing == 2)
        {
            lv2_atom_sequence_append_event(self->port_events_out3,
                                           out_capacity_3,
                                           ev);
        }
    }
}

static void cleanup(LV2_Handle instance)
{
    free(instance);
}

static const LV2_Descriptor descriptor = {
    .URI = "http://moddevices.com/plugins/mod-devel/MIDI-Switchbox4",
    .instantiate = instantiate,
    .connect_port = connect_port,
    .activate = activate,
    .run = run,
    .deactivate = NULL,
    .cleanup = cleanup,
    .extension_data = NULL
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index)
{
    return (index == 0) ? &descriptor : NULL;
}
